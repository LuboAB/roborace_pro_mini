#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include "web_log.h"
#include "tof.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 专用 I2C 控制器与引脚（不占用 Wire）
static TwoWire I2C_TOF = TwoWire(1); // I2C 外设1
static const int TOF_SDA = 17;
static const int TOF_SCL = 18;
static const uint32_t TOF_FREQ = 400000; // 提升到400k加快 I2C 访问
// 建议为长距离模式集中管理预算与周期
static const uint32_t TOF_BUDGET_US = 50000;   // 50 ms 预算，提升远距离稳定性
static const uint16_t TOF_PERIOD_MS = 50;      // 连续测量周期必须 >= 预算

// 多传感器配置
static const int NUM_TOF = 3;
static VL53L1X s_tofs[NUM_TOF];
static const int TOF_XSHUT[NUM_TOF] = {5, 6, 7};
static const uint8_t TOF_ADDR[NUM_TOF] = {0x2A, 0x2B, 0x2D}; // 运行期地址
static volatile uint16_t s_dist_mm[NUM_TOF] = {0};
static TaskHandle_t s_tofTask = nullptr;

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

static void tof_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10); // 100 Hz
    for (;;)
    {
        for (int i = 0; i < NUM_TOF; ++i)
        {
            bool ready = true;
#if 1
            ready = s_tofs[i].dataReady();
#endif
            if (ready)
            {
                uint16_t d = s_tofs[i].read(); // 连续模式下读取最新值
                
                if (!s_tofs[i].timeoutOccurred())
                    s_dist_mm[i] = d; // 原子写16位，ESP32上可接受
            }
        }
        vTaskDelayUntil(&last, period);
    }
}

void tof_setup()
{
    tof_power_all_off();
    delay(10);
    scanBus(I2C_TOF, "I2C_TOF");
    I2C_TOF.begin(TOF_SDA, TOF_SCL, TOF_FREQ);
    addLog("I2C_TOF begun");
    scanBus(I2C_TOF, "I2C_TOF");

    for (int i = 0; i < NUM_TOF; ++i)
    {
        digitalWrite(TOF_XSHUT[i], HIGH);
        delay(50); // 给足启动时间

        s_tofs[i].setTimeout(500);
        s_tofs[i].setBus(&I2C_TOF);

        if (!s_tofs[i].init())
        {
            addLog("VL53L1X #" + String(i) + " init failed");
            continue;
        }

        s_tofs[i].setDistanceMode(VL53L1X::Long);
        s_tofs[i].setMeasurementTimingBudget(TOF_BUDGET_US); // 适配长距离

        s_tofs[i].setAddress(TOF_ADDR[i]);
        delay(10);

        s_tofs[i].startContinuous(TOF_PERIOD_MS); // ≈20 Hz
        addLog("VL53L1X #" + String(i) + " started @0x" + String(TOF_ADDR[i], HEX));
    }

    if (s_tofTask == nullptr)
    {
        xTaskCreatePinnedToCore(
            tof_task, "tof100Hz", 4096, nullptr,
            4, &s_tofTask, 1 /* Core 1 */
        );
    }
}

bool tof_read_all(uint16_t *out, size_t out_len)
{
    if (!out || out_len < (size_t)NUM_TOF)
        return false;
    for (int i = 0; i < NUM_TOF; ++i)
        out[i] = s_dist_mm[i];
    return true;
}