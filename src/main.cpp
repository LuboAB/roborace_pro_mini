#include <Arduino.h>
#include <Servo.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

#include "test.h"
#include "web_log.h"
#include "tof.h" // 新增
#include "driver/mcpwm.h"

#define OLED_SDA 10
#define OLED_SCL 9
#define OLED_RESET 4

Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

Servo myservo;
float servoPos = 80;
// 使用 MCPWM 输出 10kHz
#define MOTOR_PWM_PIN 14

void pwm_init() {
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, MOTOR_PWM_PIN);
    mcpwm_config_t cfg = {};
    cfg.frequency = 10000;                // 10 kHz
    cfg.cmpr_a = 0;                       // 初始占空比 0%
    cfg.counter_mode = MCPWM_UP_COUNTER;
    cfg.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
}

void pwm_set_duty(float duty) {
    if (duty < 0) duty = 0;
    if (duty > 1) duty = 1;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty * 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}

void set_speed(float speed)
{
    speed*=0.3; // 限制最大速度为 30%
    if (speed > 0)
    {
        float duty = speed;
        pwm_set_duty(duty);
        digitalWrite(13, HIGH); // 正转
    }
    else
    {
        float duty = -speed;
        pwm_set_duty(duty);
        digitalWrite(13, LOW); // 反转
    }
}
void oled_init();
void setup()
{
    Serial.begin(9600);

    myservo.attach(12, 500, 2500);
    myservo.write(g_servoPos);
    addLog("Servo attached at " + String(g_servoPos));

    pinMode(13, OUTPUT);
    web_log_start();
    addLog("Boot");

    oled_init();
    addLog("OLED inited");

    // 调用独立的 ToF 初始化（包含 I2C 与多传感器上电改址）
    tof_setup();


    pwm_init();
    addLog("PWM 10kHz inited");
    pwm_set_duty(0.3f); // 示例：50% 占空比
}

//30  80  120
void loop()
{
    // 真正执行硬件动作
    //myservo.write(g_servoPos);
    set_speed(g_speed);
   
    uint16_t dist[2] = {0};
     tof_read_all(dist, 2);
     g_distance1 = dist[0];
     g_distance2 = dist[1];


    int L=dist[0];
    if(L>2800) L=3000;
    int R=dist[1];
    if(R>2800) R=3000;
    int error =  R-L;
    float Kp = 0.07;
    servoPos = 80 + error * Kp;
    if (servoPos < 30) servoPos = 30;
    if (servoPos > 120) servoPos = 120;
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

void oled_init()
{
    Wire.begin(OLED_SDA, OLED_SCL); // 或 Wire.begin(OLED_SDA, OLED_SCL, 400000);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextColor(WHITE);
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("ready");
    display.display();
    addLog("OLED display ready");
}

// 在需要调节电机功率的位置调用：pwm_set_duty(占空比);