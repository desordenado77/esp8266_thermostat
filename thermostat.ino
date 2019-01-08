#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <time.h>


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





ESP8266WebServer server(80);

WiFiServer telnet(23);
WiFiClient telnetClients;
int disconnectedClient = 1;

int enableTime = 0;
time_t now; 

#define DEBUG_LOG_LN(x) { Serial.println(x); if(telnetClients) { telnetClients.println(x); } }
#define DEBUG_LOG(x) { Serial.print(x); if(telnetClients) { telnetClients.print(x); } }

#define DEBUG_LOG_INFO_LN(x) { if(enableTime) { now = time(nullptr); DEBUG_LOG(now); DEBUG_LOG(":"); DEBUG_LOG(__LINE__); DEBUG_LOG(":"); DEBUG_LOG_LN(x); } }
#define DEBUG_LOG_INFO(x) { if(enableTime) { now = time(nullptr); DEBUG_LOG(now); DEBUG_LOG(":"); DEBUG_LOG(__LINE__); DEBUG_LOG(":"); DEBUG_LOG(x); } }

//define your default values here, if there are different values in config.json, they are overwritten.
char client_secret[128] = "";
char client_id[128] = "";
char refresh_token[128] = "";
char access_token[128] = "";
char device_id[128] = "";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_LOG_INFO_LN("Should save config");
  shouldSaveConfig = true;
}

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleDisconnect(){
  WiFi.disconnect();
  ESP.restart();
}


void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setup() {
  Serial.begin(115200);
  
  dht.begin();
  
  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  pinMode(CURSOR_BUTTON, INPUT_PULLUP);
  pinMode(CURSOR_UP, INPUT_PULLUP);
  pinMode(CURSOR_DOWN, INPUT_PULLUP);

  DEBUG_LOG_INFO_LN();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  DEBUG_LOG_INFO_LN("mounting FS...");

  if (SPIFFS.begin()) {
    DEBUG_LOG_INFO_LN("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DEBUG_LOG_INFO_LN("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DEBUG_LOG_INFO_LN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          DEBUG_LOG_INFO_LN("parsed json");

          strcpy(client_secret, json["client_secret"]);
          strcpy(client_id, json["client_id"]);
          strcpy(refresh_token, json["refresh_token"]);
          strcpy(device_id, json["device_id"]);

        } else {
          DEBUG_LOG_INFO_LN("failed to load json config");
        }
      }
    }
  } else {
    DEBUG_LOG_INFO_LN("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_client_secret("client_secret", "client_secret", client_secret, 128);
  WiFiManagerParameter custom_client_id("client_id", "client_id", client_id, 128);
  WiFiManagerParameter custom_refresh_token("refresh_token", "refresh_token", refresh_token, 128);
  WiFiManagerParameter custom_device_id("device_id", "device_id", device_id, 128);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_client_secret);
  wifiManager.addParameter(&custom_client_id);
  wifiManager.addParameter(&custom_refresh_token);
  wifiManager.addParameter(&custom_device_id);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    DEBUG_LOG_INFO_LN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  DEBUG_LOG_INFO_LN("connected...yeey :)");

  //read updated parameters
  strcpy(client_secret, custom_client_secret.getValue());
  strcpy(client_id, custom_client_id.getValue());
  strcpy(refresh_token, custom_refresh_token.getValue());
  strcpy(device_id, custom_device_id.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DEBUG_LOG_INFO_LN("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["client_secret"] = client_secret;
    json["client_id"] = client_id;
    json["refresh_token"] = refresh_token;
    json["device_id"] = device_id;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DEBUG_LOG_INFO_LN("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  DEBUG_LOG_INFO_LN("local ip");
  DEBUG_LOG_INFO_LN(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    DEBUG_LOG_LN("MDNS responder started");
    
    MDNS.addService("http", "tcp", 80);
    
    MDNS.addService("telnet", "tcp", 23);
  }
  else {
    DEBUG_LOG_LN("MDNS responder failed");    
  }

  server.on("/", handleRoot);
  server.on("/disconnectWifi", handleDisconnect);
  
  server.onNotFound(handleNotFound);

  server.begin();
  DEBUG_LOG_LN("HTTP server started");  

  telnet.begin();
  telnet.setNoDelay(true);
  DEBUG_LOG_INFO_LN("Telnet server started");  

  configTime(0, 0, "2.es.pool.ntp.org", "1.europe.pool.ntp.org", "2.europe.pool.ntp.org");
  Serial.println("Waiting for time");
  DEBUG_LOG_INFO(" ");
  while (!time(nullptr)) {
    DEBUG_LOG(".");
    delay(1000);
  }
  DEBUG_LOG_LN("");
  now = time(nullptr);
  // This time is not always correct... but who cares.., later it becomes correct
  DEBUG_LOG_LN(ctime(&now));
  

  enableTime = 1;
  
  // WiFi.disconnect();
}




void showError(String str) {
  display.drawString(0, 40, str);
  DEBUG_LOG_INFO_LN(str);

}

void showSensor(float h, float t){
  display.drawString(0, 50, "H: " + String(h) + "T: " + String(t));
  
  DEBUG_LOG_INFO_LN("H: " + String(h) + "T: " + String(t));
}




void loop() {
  uint8_t i;
  static int once = 0;
  server.handleClient();
  time_t currentTime = time(nullptr);
  int error = 0;


  if (telnet.hasClient()) {
    if (!telnetClients || !telnetClients.connected()) {
      if (telnetClients) {
        telnetClients.stop();
        DEBUG_LOG_INFO_LN("Telnet Client Stop");
      }
      telnetClients = telnet.available();
      DEBUG_LOG_INFO_LN("New Telnet client");
      telnetClients.flush();  // clear input buffer, else you get strange characters 
      disconnectedClient = 0;
    }
  }
  else {
    if(!telnetClients.connected()) {
      if(disconnectedClient == 0) {
        DEBUG_LOG_INFO_LN("Client Not connected");
        telnetClients.stop();
        disconnectedClient = 1;
      }
    }
  }

  MDNS.update();

  // clear the display
  display.clear();

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


  DEBUG_LOG_INFO("Memory: ");
  DEBUG_LOG_LN(ESP.getFreeHeap());
  
}

