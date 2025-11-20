#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include "web_log.h"
#include "tof.h"

// 专用 I2C 控制器与引脚（不占用 Wire）
static TwoWire I2C_TOF = TwoWire(1); // I2C 外设1
static const int TOF_SDA = 17;
static const int TOF_SCL = 18;
static const uint32_t TOF_FREQ = 100000; // 100k 提升稳定性

// 多传感器配置
static const int NUM_TOF = 2;
static VL53L1X s_tofs[NUM_TOF];
static const int TOF_XSHUT[NUM_TOF] = {5 , 6};
static const uint8_t TOF_ADDR[NUM_TOF] = {0x2A , 0x2B }; // 运行期地址

void scanBus(TwoWire &bus, const char *name)
{
    addLog(String("Scan ") + name);
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        bus.beginTransmission(addr);
        if (bus.endTransmission() == 0)
        {
            Serial.printf("[%s] Found 0x%02X\n", name, addr);
            addLog(String(name) + " dev 0x" + String(addr, HEX));
            delay(2);
        }
    }
}

size_t tof_count() { return NUM_TOF; }

static void tof_power_all_off()
{
    for (int i = 0; i < NUM_TOF; ++i)
    {
        pinMode(TOF_XSHUT[i], OUTPUT);
        digitalWrite(TOF_XSHUT[i], LOW);
    }
}

void tof_setup()
{
    tof_power_all_off();
    delay(10);

    I2C_TOF.begin(TOF_SDA, TOF_SCL, TOF_FREQ);
    addLog("I2C_TOF begun");
    scanBus(I2C_TOF, "I2C_TOF");

    // s_tofs[0].setTimeout(500);
    // s_tofs[0].setDistanceMode(VL53L1X::Short);
    // s_tofs[0].setMeasurementTimingBudget(50000); // 50ms 预算
    // s_tofs[0].startContinuous(60);               // 60ms 周期

    // 逐个上电 -> init(默认0x29) -> 改地址 -> 连续测距
    for (int i = 0; i < NUM_TOF; ++i) {
        digitalWrite(TOF_XSHUT[i], HIGH);
        delay(50); // 给足启动时间

        s_tofs[i].setTimeout(500);
        // Pololu VL53L1X 库支持切换到自定义 I2C 实例
        s_tofs[i].setBus(&I2C_TOF);

        if (!s_tofs[i].init()) {
            addLog("VL53L1X #" + String(i) + " init failed");
            continue;
        }

        // 设置距离模式与测量时序（与周期匹配）
        // Short 对强环境光更稳，Long 可更远；可按需要调整
        s_tofs[i].setDistanceMode(VL53L1X::Short);
        s_tofs[i].setMeasurementTimingBudget(50000); // 50ms 预算

        // 改运行期 I2C 地址
        s_tofs[i].setAddress(TOF_ADDR[i]);
        delay(10);

        // 周期 >= 预算，避免阻塞/超时
        s_tofs[i].startContinuous(60); // 60ms 周期
        addLog("VL53L1X #" + String(i) + " started @0x" + String(TOF_ADDR[i], HEX));
    }
}

bool tof_read_all(uint16_t *out, size_t out_len)
{
    if (!out || out_len < (size_t)NUM_TOF)
        return false;
    bool ok = true;
    for (int i = 0; i < NUM_TOF; ++i)
    {
        // read() 在连续模式下会读取最新结果，可能阻塞直到数据就绪
        uint16_t d = s_tofs[i].read(); // mm
        out[i] = d;
        if (s_tofs[i].timeoutOccurred())
        {
            // addLog("VL53L1X #" + String(i) + " timeout");
            ok = false;
        }
    }
    return ok;
}