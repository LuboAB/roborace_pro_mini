#include <Arduino.h>
#include <Servo.h>

#include "test.h"
#include "web_log.h"
#include "tof.h"
#include "oled_debug.h"
#include "motor.h"


Servo myservo;
volatile float servoPos = 80;
void setup()
{
    Serial.begin(115200);

    myservo.attach(12, 500, 2500);
    myservo.write(g_servoPos);
    addLog("Servo attached at " + String(g_servoPos));

    web_log_start();
    addLog("Boot");

    oled_init();
    addLog("OLED inited");

    // 调用独立的 ToF 初始化（包含 I2C 与多传感器上电改址）
    tof_setup();

    motor_init();
    addLog("PWM 10kHz inited");
}

// 30  80  120
void loop()
{
    // 读取当前编码器计数
    // g_encoderCount = g_encoder.getCount();

    // myservo.write(g_servoPos);
    // set_speed(g_speed);
    uint16_t dist[3] = {0};
    uint8_t status[3] = {0};
    tof_read_all_with_status(dist, status, 3);

    g_distance1 = L;
    g_distance2 = R;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("ENC: ");
    display.println((int)g_encoderCount); // 显示编码器计数
    display.print("target: ");
    display.println(target);
    display.print("servo: ");
    display.println(servoPos);
    display.print("D0: ");
    display.print(L);
    display.println(" mm");
    display.print("D1: ");
    display.print(R);
    display.println(" mm");
    display.print("D2: ");
    display.print(F);
    display.println(" mm");
    display.print(" status:");
    display.println(status[2]);
    display.display();
}

void test_function()
{

    uint16_t dist[3] = {0};
    tof_read_all(dist, 2);
    g_distance1 = dist[0];
    g_distance2 = dist[1];

    int L = dist[0];
    if (L > 2800)
        L = 3000;
    int R = dist[1];
    if (R > 2800)
        R = 3000;
    int error = R - L;
    float Kp = 0.07;
    servoPos = 80 + error * Kp;
    if (servoPos < 30)
        servoPos = 30;
    if (servoPos > 120)
        servoPos = 120;
    myservo.write(servoPos);

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("servo: ");
    display.println(g_servoPos);
    display.print("speed: ");
    display.println(g_speed);
    display.print("err: ");
    display.println(error);
    display.setTextSize(1);
    // display.setCursor(0, 24);
    display.print("D0: ");
    display.print(dist[0]);
    display.println(" mm");
    display.setCursor(0, 36);
    display.print("D1: ");
    display.print(dist[1]);
    display.println(" mm");
    display.display();
    // delay(20);
}