/* ATM90E26 Energy Monitor Demo Application

    The MIT License (MIT)

  Copyright (c) 2016 whatnick and Ryzee

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// the sensor communicates using SPI, so include the library:
#include <FS.h> //this needs to be first, or it all crashes and burns..

#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

const char* host = "din-energy";
const char* update_path = "/firmware";
const char* update_username = "whatnick";
const char* update_password = "energy";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

//flag for saving data
bool shouldSaveConfig = false;

//Thingspeak config
char server[50] = "api.thingspeak.com";
// Sign up on thingspeak and get WRITE API KEY.
char auth[36] = "THINGSPEAK_KEY";
//Wifi Client to send data to Thingspeak
WiFiClient client;

#include <SPI.h>

/*******************
   WEMOS SPI Pins:
   SCLK - D5
   MISO - D6
   MOSI - D7
   SS   - D8
 *******************/
#include "energyic_SPI.h"
#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

//Debug stuff with out debug printing is disabled
//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif

U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0);   // EastRising 0.66" OLED breakout board, Uno: A4=SDA, A5=SCL, 5V powered

#define whatnick_logo_width 64
#define whatnick_logo_height 45
static unsigned char whatnick_logo_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38,
  0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x1e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x3e, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f,
  0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x39, 0xc6, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe0, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01,
  0xc0, 0x03, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xc3, 0xe1, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x30, 0xe3, 0x63, 0x06, 0x00, 0x00, 0x00, 0x00, 0x10, 0xe7,
  0x73, 0x04, 0x00, 0x00, 0x00, 0x00, 0x18, 0xce, 0x39, 0x0c, 0x00, 0x00,
  0x00, 0x00, 0x18, 0xce, 0x3c, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x18, 0x3c,
  0x1e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x18, 0xfc, 0x1f, 0x0c, 0x00, 0x00,
  0x00, 0x00, 0x18, 0xf8, 0x0f, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x18, 0xf8,
  0x0f, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x10, 0xf8, 0x0f, 0x06, 0x00, 0x00,
  0x00, 0x00, 0x30, 0xf8, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00, 0x30, 0xf0,
  0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x60, 0xf0, 0x07, 0x03, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0xf0, 0x87, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe1,
  0xe3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x71, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x3e, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
  0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x17, 0x82, 0xe1,
  0x27, 0x4c, 0x7c, 0x32, 0x22, 0x13, 0x82, 0x81, 0x61, 0x48, 0x06, 0x12,
  0x62, 0x13, 0x82, 0x81, 0xe1, 0x48, 0x02, 0x0e, 0x72, 0xf1, 0xc3, 0x82,
  0xa1, 0x49, 0x02, 0x0e, 0xdc, 0x31, 0xe3, 0x83, 0x21, 0x4f, 0x02, 0x1a,
  0xdc, 0x11, 0x62, 0x87, 0x21, 0x4f, 0x06, 0x32, 0x9c, 0x10, 0x32, 0x84,
  0x21, 0x4e, 0x04, 0x62, 0x8c, 0x10, 0x12, 0x8c, 0x21, 0x4c, 0x78, 0x62
};

ATM90E26_SPI eic1(D8);
ATM90E26_SPI eic2(D3);

//Variables and timers
float v1, i1, r1, pf1, v2, i2, r2, pf2;
short st1, st2;
int sampleCount = 0;
long curMillis, prevMillis;

//callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}

void readTSConfig()
{
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  //DEBUG_PRINTLN("mounting FS...");

  if (SPIFFS.begin()) {
    //DEBUG_PRINTLN("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      //DEBUG_PRINTLN("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //DEBUG_PRINTLN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          //DEBUG_PRINTLN("\nparsed json");
          strcpy(auth, json["auth"]);
          strcpy(server, json["server"]);
        } else {
          //DEBUG_PRINTLN("failed to load json config");
        }
      }
    }
  } else {
    //DEBUG_PRINTLN("failed to mount FS");
  }
  //end read
}

void saveTSConfig()
{
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    //DEBUG_PRINTLN("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["auth"] = auth;
    json["server"] = server;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      //DEBUG_PRINTLN("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void setup() {

#ifdef DEBUG
  /* Initialize the serial port to host */
  Serial.begin(115200);
#endif

  //Read previous config
  readTSConfig();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_ts_token("ts", "Thingspeak Key", auth, 33);
  WiFiManagerParameter custom_server("serv", "Server", server, 50);

  //Use wifi manager to get config
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_ts_token);
  wifiManager.addParameter(&custom_server);

  //first parameter is name of access point, second is the password
#ifndef DEBUG
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("EnergyMonitor", "whatnick");
#else
  wifiManager.startConfigPortal("EnergyMonitor", "whatnick");
#endif

  //if you get here you have connected to the WiFi
  DEBUG_PRINTLN("connected...yeey :)");
  DEBUG_PRINT("Key:");
  DEBUG_PRINTLN(auth);
  DEBUG_PRINT("Server:");
  DEBUG_PRINTLN(server);

  //read updated parameters
  strcpy(auth, custom_ts_token.getValue());
  strcpy(server, custom_server.getValue());

  saveTSConfig();

  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);

  u8g2.begin();

  u8g2.firstPage();
  do {
    u8g2.drawXBM(0, 0, 64, 45, whatnick_logo_bits);
  } while ( u8g2.nextPage() );
  u8g2.setFont(u8g2_font_5x8_tr);
  delay(1000);

  /*Initialise the ATM90E26 + SPI port */
  eic1.SetLGain(0x240b);
  eic1.SetUGain(0x6810);
  eic1.SetIGain(0x7644);
  eic1.SetCRC1(0x410D);
  eic1.SetCRC2(0x0FD7);
  int stat1 = eic1.GetMeterStatus();
  DEBUG_PRINT("Meter 1 Status:");
  DEBUG_PRINTLN(stat1);
  eic1.InitEnergyIC();
  //Hack meter 1 needs to initialized twice some SPI bus startup bug on ESP8266.
  delay(100);
  eic1.InitEnergyIC();

  eic2.SetLGain(0x240b);
  eic2.SetUGain(0x6720);
  eic2.SetIGain(0x7644);
  eic2.SetCRC1(0x410D);
  eic2.SetCRC2(0x30E6);
  int stat2 = eic1.GetMeterStatus();
  DEBUG_PRINT("Meter 2 Status:");
  DEBUG_PRINTLN(stat2);
  eic2.InitEnergyIC();

}

void sendThingSpeak() {
  //TODO: Compute averages from last n-readings
  float Vrms1 = v1;
  float realPower1 = r1;
  float Crms1 = i1;
  float powerFactor1 = pf1;

  float Vrms2 = v2;
  float realPower2 = r2;
  float Crms2 = i2;
  float powerFactor2 = pf2;

  if (client.connect(server, 80)) { //   "184.106.153.149" or api.thingspeak.com
    String postStr = String(auth);
    postStr += "&field1=";
    postStr += String(Vrms1);
    postStr += "&field2=";
    postStr += String(realPower1);
    postStr += "&field3=";
    postStr += String(Crms1);
    postStr += "&field4=";
    postStr += String(powerFactor1);
    postStr += "&field5=";
    postStr += String(Vrms2);
    postStr += "&field6=";
    postStr += String(realPower2);
    postStr += "&field7=";
    postStr += String(Crms2);
    postStr += "&field8=";
    postStr += String(powerFactor2);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + String(auth) + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
  }
  client.stop();
}

void readMeterDisplay()
{
  st1 = eic1.GetMeterStatus();
  v1 = eic1.GetLineVoltage();
  i1 = eic1.GetLineCurrent();
  r1 = eic1.GetActivePower();
  pf1 = eic1.GetPowerFactor();

  st2 = eic2.GetMeterStatus();
  v2 = eic2.GetLineVoltage();
  i2 = eic2.GetLineCurrent();
  r2 = eic2.GetActivePower();
  pf2 = eic2.GetPowerFactor();

  u8g2.clearDisplay();
  u8g2.firstPage();
  do {
    u8g2.drawStr(0, 9, "V:");
    u8g2.setCursor(12, 9);
    u8g2.print(v1, 1);
    u8g2.setCursor(40, 9);
    u8g2.print(v2, 1);

    u8g2.drawStr(0, 18, "I:");
    u8g2.setCursor(12, 18);
    u8g2.print(i1, 2);
    u8g2.setCursor(40, 18);
    u8g2.print(i2, 2);

    u8g2.drawStr(0, 27, "P:");
    u8g2.setCursor(12, 27);
    u8g2.print(r1, 0);
    u8g2.setCursor(40, 27);
    u8g2.print(r2, 0);

    u8g2.drawStr(0, 36, "F:");
    u8g2.setCursor(12, 36);
    u8g2.print(pf1, 2);
    u8g2.setCursor(40, 36);
    u8g2.print(pf2, 2);

    u8g2.drawStr(0, 45, "ST:");
    u8g2.setCursor(12, 45);
    u8g2.print(st1);
    u8g2.setCursor(40, 45);
    u8g2.print(st2);

  } while ( u8g2.nextPage() );
  prevMillis = curMillis;
}

void loop() {
  /*Repeatedly fetch some values from the ATM90E26 */
  curMillis = millis();
  httpServer.handleClient();
  yield();
  if (sampleCount > 20)
  {
    sendThingSpeak();
    sampleCount = 0;
  }
  if ((curMillis - prevMillis) > 500)
  {
    u8g2.clearDisplay();
  }
  if ((curMillis - prevMillis) > 1000)
  {
    readMeterDisplay();
    sampleCount++;
  }
}
