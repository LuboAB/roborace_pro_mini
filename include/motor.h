#pragma once
#include <Arduino.h>
#include "driver/mcpwm.h"
#include <ESP32Encoder.h>
#include "web_log.h"
#include "tof.h"
#include <Servo.h>

#define MOTOR_PWM_PIN 14

extern ESP32Encoder g_encoder;
extern volatile int64_t g_encoderCount;
extern volatile float target;

extern volatile int L;
extern volatile int R;
extern volatile int F;
void motor_init();
void pwm_set_duty(float duty);
void set_speed(float speed); // speed 范围 -1.0 到 1.