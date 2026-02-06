#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#define SCREEN_WIDTH  128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI  4
#define OLED_CLK   3
#define OLED_DC    6
#define OLED_CS    12
#define OLED_RESET 5
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET,
                         OLED_CS);

/* Comment out above, uncomment this block to use hardware SPI
  #define OLED_DC     6
  #define OLED_CS     7
  #define OLED_RESET  8
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  &SPI, OLED_DC, OLED_RESET, OLED_CS);
*/

#define NUMFLAKES 3 // Number of snowflakes in the animation example

#define LOGO_HEIGHT 16
#define LOGO_WIDTH  16

char inChar;
String string;
int linenumber = 0;

void setup()
{
    Serial.begin(115200);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    // Clear the buffer
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.setCursor(30, 18);
    display.print("EARN-E");
    display.setCursor(5, 36);
    display.print("programmer");
    display.display();
    delay(3000);
    // display.display() is NOT necessary after every single drawing command,
    // unless that's what you want...rather, you can batch up a bunch of
    // drawing operations and then update the screen all at once by calling
    // display.display(). These examples demonstrate both approaches...
}

void loop()
{
    if (Serial.available())
    {
        if (linenumber == 0)
        {
            display.clearDisplay();
        }
        inChar = Serial.read();
        string += inChar;
        display.setCursor(0, linenumber * 16);
        display.print(string);
        display.display();
        if (byte(inChar) == 0x0D)
        {
            display.display();
            // Serial.println(linenumber);
            linenumber += 1;
            if (linenumber > 3)
            {
                linenumber = 0;
            }
            string = "";
        }
    }
    // display.display();
}
