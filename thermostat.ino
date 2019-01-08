// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

/*
https://github.com/spacehuhn/esp8266_deauther/wiki/Setup-Display-&-Buttons
*/

// This DHT sensor library fixes a bug from adafruit library:
// https://github.com/khjoen/DHT-sensor-library


#include "DHT.h"
 
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
 
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
 
const int DHTPin = 3;     // what digital pin we're connected to. This corresponds to D9 in the wemos board.


 
DHT dht(DHTPin, DHTTYPE);
 

#define CURSOR_BUTTON 14
#define CURSOR_UP     12
#define CURSOR_DOWN   13


SSD1306Wire  display(0x3c, 5 /*D1*/, 4 /*D2*/);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  dht.begin();
  
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

void showError(String str) {
  display.drawString(0, 40, str);
}

void showSensor(float h, float t){
  display.drawString(0, 50, "H: " + String(h) + "T: " + String(t));
}

void loop() {
  // clear the display
  display.clear();

  showPins(digitalRead(CURSOR_BUTTON), digitalRead(CURSOR_UP), digitalRead(CURSOR_DOWN));

//  delay(10);
  // Wait a few seconds between measurements.
  delay(2000);
  
  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  static float prevH, prevT;

  
  if (isnan(h) || isnan(t)) {
    showError("Failed to read from DHT sensor!");
    showSensor(prevH, prevT);
//    return;
  }
  else {

  
    showSensor(h, t);
    prevH = h;
    prevT = t;
  }
  
  display.display();  
}
