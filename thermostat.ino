// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

/*
https://github.com/spacehuhn/esp8266_deauther/wiki/Setup-Display-&-Buttons
*/

#define CURSOR_BUTTON 14
#define CURSOR_UP     12
#define CURSOR_DOWN   13


SSD1306Wire  display(0x3c, 5 /*D1*/, 4 /*D2*/);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();


  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  pinMode(CURSOR_BUTTON, INPUT_PULLUP);
  pinMode(CURSOR_UP, INPUT_PULLUP);
  pinMode(CURSOR_DOWN, INPUT_PULLUP);

}

void showPins(int button, int up, int down) {
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);


  if(!up) 
    display.drawString(0, 10, "UP is Pressed");
  if(!down) 
    display.drawString(0, 20, "DOWN is Pressed");
  if(!button) 
    display.drawString(0, 30, "BUTTON is Pressed");

}

void loop() {
  // clear the display
  display.clear();

  showPins(digitalRead(CURSOR_BUTTON), digitalRead(CURSOR_UP), digitalRead(CURSOR_DOWN));

  display.display();
  delay(10);
}

