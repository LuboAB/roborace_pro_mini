#include "oled_debug.h"


Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

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