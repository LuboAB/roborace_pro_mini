#include "test.h"
int low = 10;
int high = 125;

void test_servo(Adafruit_SSD1306 display, Servo myservo)
{
    for (int i = low; i < high; i++)
    {
        myservo.write(i);
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println(i);
        display.display();
        delay(50);
    }
    delay(500);
    for (int i = high; i > low; i--)
    {
        myservo.write(i);
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println(i);
        display.display();
        delay(50);
    }
    delay(500);
}