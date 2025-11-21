#pragma once
#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "web_log.h"

#define OLED_SDA 10
#define OLED_SCL 9
#define OLED_RESET 4

extern Adafruit_SSD1306 display;

void oled_init();