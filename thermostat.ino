/*
 * Description of the board from Aliexpress, for possible future reference
 * Features:
 * One AD inputs.
 * Micro USB inputs.
 * One programmable LED (D0).
 * Integrated 18650 battery charging and discharging system.
 * One switch controls whether the 18650 battery is powered or not.
 * OLED's SDA and SCL connect to the D1 pin and the D2 pin respectively.
 * The five buttons are controlled by FLATH, RSET, D5, D6, and D7 respectively.
 * The 5 Digital pins can configure the write/read/interrupt/pwm/I2C/one-wire supported separately.
 * Operation and NodeMCU consistent, adding a programmable LED, you can use GPIO16 to control, display 8266 running status and other functions. Integrated OLED and five button, more convenient for development.
 * The design concept originates from the open source project NodeMCU, and the development board integrates 18650 charging and discharging systems with charging and discharging protection. At the same time, a OLED and five directional buttons are integrated to facilitate the development.
 * 
 * 
 * 
 * 
 */


#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>          //Version 5 of https://github.com/bblanchon/ArduinoJson

#include <time.h>

#include <mDNSResolver.h>       //https://github.com/madpilot/mDNSResolver

using namespace mDNSResolver;
WiFiUDP udp;
Resolver resolver(udp);
IPAddress RelayIP (0, 0, 0, 0);


// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`   // https://github.com/ThingPulse/esp8266-oled-ssd1306

/*
https://github.com/spacehuhn/esp8266_deauther/wiki/Setup-Display-&-Buttons
*/

// This DHT sensor library fixes a bug from adafruit library:
// https://github.com/khjoen/DHT-sensor-library

// graphics come from https://github.com/ThingPulse/esp8266-oled-ssd1306

#include "DHT.h" // https://github.com/khjoen/DHT-sensor-library
 
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
 
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
 
const int DHTPin = 3;     // what digital pin we're connected to. This corresponds to D9 in the wemos board.
 
DHT dht(DHTPin, DHTTYPE);

SSD1306Wire  display(0x3c, 5 /*D1*/, 4 /*D2*/);


#define CURSOR_BUTTON   14
#define CURSOR_UP       12
#define CURSOR_DOWN     13




#define TIME_FOR_ACTION     (10*60)
#define ERRORS_TO_REPORT    10


float targetTemp =      22;
int targetTime =        0;
int targetTimeOrig =    0;
int updateTargetTime =  0;
float prevH, prevT;
int acOn =              0;
int acMode =            0;
int retryControl =      0;
int controlErrors =     0;
time_t timeUpdate =     0;
time_t lastKeyTime =    0;
int showDisplay =       0;

#define ACMODE_TEMP           0
#define ACMODE_TIME           1
#define TIME_INCREMENTS       15
#define TIME_TO_SHOW_DISPLAY  (2*60) 

#define AP_SSID               "THERMOSTAT_AP"

ESP8266WebServer server(80);

WiFiServer telnet(23);
WiFiClient telnetClients;
int disconnectedClient = 1;


int enableTime = 0;
time_t now; 

#define DEBUG_LOG_LN(x) { Serial.println(x); if(telnetClients) { telnetClients.println(x); } }
#define DEBUG_LOG(x) { Serial.print(x); if(telnetClients) { telnetClients.print(x); } }

#define DEBUG_LOG_INFO_LN(x) { if(enableTime) { now = time(nullptr); DEBUG_LOG(now); DEBUG_LOG(":"); } DEBUG_LOG(__LINE__); DEBUG_LOG(":"); DEBUG_LOG_LN(x); }
#define DEBUG_LOG_INFO(x) { if(enableTime) { now = time(nullptr); DEBUG_LOG(now); DEBUG_LOG(":"); } DEBUG_LOG(__LINE__); DEBUG_LOG(":"); DEBUG_LOG(x); }



//define your default values here, if there are different values in config.json, they are overwritten.
char relayName[128] = "";



//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_LOG_INFO_LN("Should save config");
  shouldSaveConfig = true;
}


void configModeCallback (WiFiManager *myWiFiManager) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "Entered config mode");
  display.drawString(0, 25, myWiFiManager->getConfigPortalSSID());
  display.display();
}


void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleDisconnect(){
  server.send(200, "text/plain", "Done");
  DEBUG_LOG_INFO_LN("Disconnecting");
  WiFi.disconnect();
  DEBUG_LOG_INFO_LN("Restarting");
  ESP.restart();
}

void handleTemp() {
  server.send(200, "text/plain", String(prevT));  
}


void handleHumidity() {
  server.send(200, "text/plain", String(prevH));  
}

void handleTargetTemp() {
  for (int i = 0; i < server.args(); i++) {
    if(server.argName(i) == "temp") {
      targetTemp = server.arg(i).toFloat();
      break;
    }
  } 

  server.send(200, "text/plain", "Done");
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
  display.setFont(ArialMT_Plain_16);

  display.drawString(0, 20, "Connecting to Wifi");

  display.display();  
  
  pinMode(CURSOR_BUTTON, INPUT_PULLUP);
  pinMode(CURSOR_UP, INPUT_PULLUP);
  pinMode(CURSOR_DOWN, INPUT_PULLUP);

  DEBUG_LOG_INFO_LN();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  DEBUG_LOG_INFO_LN("mounting FS...");

  // Need to set the SPIFFS size in arduino ide for this to work
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

          strcpy(relayName, json["relayName"]);

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
  WiFiManagerParameter custom_relayName("relayName", "relayName", relayName, 128);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.setAPCallback(configModeCallback);  

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_relayName);

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
  if (!wifiManager.autoConnect(AP_SSID)) {
    DEBUG_LOG_INFO_LN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  DEBUG_LOG_INFO_LN("connected...yeey :)");

  //read updated parameters
  strcpy(relayName, custom_relayName.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DEBUG_LOG_INFO_LN("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["relayName"] = relayName;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DEBUG_LOG_INFO_LN("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  display.clear();
  display.drawString(0, 20, "Connected");
  display.drawString(0, 40, "IP: " + WiFi.localIP().toString());
  display.display();  
  

  DEBUG_LOG_INFO_LN("local ip");
  DEBUG_LOG_INFO_LN(WiFi.localIP());

  if (MDNS.begin("thermostat")) {
    DEBUG_LOG_LN("MDNS responder started");
    
    MDNS.addService("http", "tcp", 80);
    
    MDNS.addService("telnet", "tcp", 23);
  }
  else {
    DEBUG_LOG_LN("MDNS responder failed");    
  }

  server.on("/", handleRoot);
  server.on("/disconnectWifi", handleDisconnect);
  server.on("/v1/temp", handleTemp);
  server.on("/v1/temperature", handleTemp);
  server.on("/v1/hum", handleHumidity);
  server.on("/v1/humidity", handleHumidity);
  server.on("/v1/target", handleTargetTemp);
  server.on("/v1/targetTemp", handleTargetTemp);
  
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


  wifi_set_sleep_type(LIGHT_SLEEP_T);
  
  delay(2000);
  // WiFi.disconnect();
  display.clear();
  display.displayOff();
}




void showError(String str) {
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 59, str);
  DEBUG_LOG_INFO_LN(str);

}

void showSensor(float h, float t){
  
  if(!showDisplay) {
    return;
  }
  
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(64, 22, " " + String(t) + " ºC");
  
  display.setFont(ArialMT_Plain_16);
  if(acMode == ACMODE_TEMP) {
    display.drawString(64, 46, " " + String(targetTemp) + " ºC");
  }
  else {
    if(timeUpdate != 0) {
      targetTime = targetTimeOrig - (time(nullptr) - timeUpdate)/60;
      if(targetTime < 0) {
        targetTime = 0;
        updateTargetTime = 1;
      }
    }
    display.drawString(64, 46, " " + String(targetTime) + " min");
  }
  if(controlErrors>=ERRORS_TO_REPORT) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 59, "ERROR:RELAY CONN");    
  }
  else {
    if(acOn) {
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 59, "AC ON");
    }
  }
  display.display();
}


int keyHandler(int button, int up, int down) {
  int keyPressed = !up | !button | !down;
  if(!showDisplay && keyPressed) {
    
    DEBUG_LOG_INFO_LN("First Key");
    
    showDisplay = 1;
    display.displayOn();
    lastKeyTime = time(nullptr);
    return 0;
  }


  if(!up) {
    DEBUG_LOG_INFO_LN("UP");
    if(acMode == ACMODE_TEMP) {
      targetTemp++;
    }
    else {
      targetTime+=TIME_INCREMENTS;
      updateTargetTime = 1;
      timeUpdate = time(nullptr);
      targetTimeOrig = targetTime;
    }
  }
  if(!down) {
    DEBUG_LOG_INFO_LN("DOWN");
    if(acMode == ACMODE_TEMP) {
      targetTemp--;
    }
    else {
      targetTime-=TIME_INCREMENTS;
      if(targetTime <=0) {
        targetTime = 0;
      }
      updateTargetTime = 1;
      targetTimeOrig = targetTime;
      timeUpdate = time(nullptr);
    }
  }
  if(!button) {
    DEBUG_LOG_INFO_LN("BUTTON");
    acMode^=1;
    updateTargetTime = 1;
    targetTime = 0;
    timeUpdate = 0;
    targetTimeOrig = targetTime;
  }

  if(keyPressed) {
    lastKeyTime = time(nullptr);
    showDisplay = 1;
  }

  return keyPressed;
  
/*
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);


  if(!up) 
    display.drawString(0, 10, "UP is Pressed");
  if(!down) 
    display.drawString(0, 20, "DOWN is Pressed");
  if(!button) 
    display.drawString(0, 30, "BUTTON is Pressed");
*/
}

int turnOnOffAC(int timeOn, int onOff) {
    HTTPClient http;
    IPAddress ServerIP (0, 0, 0, 0);

    resolver.setLocalIP(WiFi.localIP());
    
    ServerIP = resolver.search(relayName);
    if (ServerIP != INADDR_NONE) {
      DEBUG_LOG_INFO_LN("Resolved relay name: "+RelayIP);
      RelayIP = ServerIP;
    } 
    else {
      DEBUG_LOG_INFO_LN("Unable to resolve relay name. Using Previous: "+RelayIP);
    }

    // somehow esp8266 is unable to resolve the name of another esp8266 device
    // String dest = "http://" + String(relayName) + "/v1/";
    String dest = "http://" + String(ServerIP[0]) + '.' + String(ServerIP[1]) + '.' + String(ServerIP[2]) + '.' + String(ServerIP[3]) + "/v1/";
  
    if(onOff) {
      DEBUG_LOG_INFO_LN("Turn AC on");  
      acOn = 1;
      dest += "on";

      if(timeOn > 0) {
        dest += "?time="+String(timeOn);
      }
    }
    else {
      DEBUG_LOG_INFO_LN("Turn AC off");  
      acOn = 0;
      dest += "off";
    }

    DEBUG_LOG_INFO_LN("Dest: " + dest);  

    http.begin(dest);     //Specify request destination
  
    int httpCode = http.GET();            //Send the request

    if(httpCode != 200) {
      DEBUG_LOG_INFO_LN("Error Received " + String(httpCode) + " " + http.getString());  
      return -1;
    }
    return 0;
}


void loop() {
  static int cnt = 0;
  uint8_t i;
  static int once = 0;
  server.handleClient();
  time_t currentTime = time(nullptr);
  static time_t lastAction = 0;
  int error = 0;
  float h = prevH;
  float t = prevT;


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

  // Wait between measurements.
  delay(500);
  if((cnt % 4) == 0) {
  // Reading temperature or humidity takes about 250 milliseconds!
    h = dht.readHumidity();
    t = dht.readTemperature();
    DEBUG_LOG_INFO_LN("H: " + String(h) + "T: " + String(t));
  }
  
  int keyPressed = keyHandler(digitalRead(CURSOR_BUTTON), digitalRead(CURSOR_UP), digitalRead(CURSOR_DOWN));

  if (isnan(h) || isnan(t)) {
    // clear the display
    display.clear();
    showError("Failed to read from DHT sensor!");
    showSensor(prevH, prevT);
//    return;
  }
  else {  
    // clear the display
    display.clear();
    showSensor(h, t);
    prevH = h;
    prevT = t;
  }
  
  cnt++;


  if(acMode == ACMODE_TEMP) {
    if(retryControl!= 0 || keyPressed || ((currentTime - lastAction) >= TIME_FOR_ACTION)) {
      retryControl = turnOnOffAC(0, targetTemp < prevT);
    
      lastAction = currentTime;
    }
  }
  else {
    if(retryControl!= 0 || updateTargetTime) {
      if(targetTime > 0) {
        retryControl = turnOnOffAC(targetTime, 1);
      }
      else {
        retryControl = turnOnOffAC(targetTime, 0);
      }
      updateTargetTime = 0;
    }
  }

  if(retryControl!= 0) {
    controlErrors++;
  }
  else {
    controlErrors = 0;
  }

  if(lastKeyTime != 0) {
    if(currentTime - lastKeyTime > TIME_TO_SHOW_DISPLAY){
      showDisplay = 0;
      display.clear();
      display.display();
      display.displayOff();
      lastKeyTime = 0;
    }
  }
  
//  DEBUG_LOG_INFO("Memory: ");
//  DEBUG_LOG_LN(ESP.getFreeHeap());
  
}
