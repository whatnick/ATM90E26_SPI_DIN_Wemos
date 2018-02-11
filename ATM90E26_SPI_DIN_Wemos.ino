/* ATM90E26 Energy Monitor Demo Application

    The MIT License (MIT)

  Copyright (c) 2016 whatnick and Ryzee
  Edited 11/2/2018 - Cory Hawkless

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define DEBUG 1

#include <ESP8266WiFi.h>
#include <FS.h> 
#include <ArduinoJson.h>  //Config storage
#include <ESP8266WebServer.h>
#include <SPI.h>
#include "energyic_SPI.h"     //SPI Metering Chip - https://github.com/whatnick/ATM90E36_Arduino

#include <U8g2lib.h>      //OLED Driver
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

extern "C" {
  #include "user_interface.h" //Enables system_get_chip_id used in SoftAP mode
}


std::unique_ptr<ESP8266WebServer> server; //Define webserver

//-----Config variables-----
  //Wifi
  char wifi_ssid[64]     = "";
  char wifi_password[64] = "";
  
  //Thingspeak
  char ts_server[50] = "api.thingspeak.com";
  char ts_auth[36] = "THINGSPEAK_KEY";// Sign up on thingspeak and get WRITE API KEY.
  
  //Calibration
  uint32_t eic1_ugain=0x6810;
  uint32_t eic1_igain=0x7644;
  uint32_t eic1_CRC1=0x410D;
  uint32_t eic1_CRC2=0x0FD7;
  
  uint32_t eic2_ugain=0x6720;
  uint32_t eic2_igain=0x7644;
  uint32_t eic2_CRC1=0x410D;
  uint32_t eic2_CRC2=0x30E6;
  
  

//--------------------------


#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif

//Metering variables and timers
ATM90E26_SPI eic1(D8);
ATM90E26_SPI eic2(D3);
float v1, i1, r1, pf1, v2, i2, r2, pf2;
short st1, st2;
int sampleCount = 0;
long curMillis, prevMillis;


//Setup OLED
U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0); // EastRising 0.66" OLED breakout board, Uno: A4=SDA, A5=SCL, 5V powered
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

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Booting...");
  
  //Show logo while wifi is set-up
  u8g2.begin();

  u8g2.firstPage();
  do {
    u8g2.drawXBM(0, 0, 64, 45, whatnick_logo_bits);
  } while ( u8g2.nextPage() );
  u8g2.setFont(u8g2_font_5x8_tr);

  readTSConfig();

  wifi_attemptToConnect();
  
  setupWebserver();
  
  DEBUG_PRINTLN("Starting metering");
  setupMetering();


}

void loop() {

  server->handleClient();       //Handle HTTP calls

  
 /*Repeatedly fetch some values from the ATM90E26 */
  curMillis = millis();
//
  yield();
  if (sampleCount > 20)
  {
//    sendThingSpeak();
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
  checkPins();
}


void checkPins(){
  if(!digitalRead(D4)){     //If button B on the oled is pressed, format the SPI file system and reset the unit - AKA Factory reset
    Serial.println("Reset pin pressed, Formatting");
    SPIFFS.format();
    Serial.println("Resetting unit");
    ESP.reset();
  }
}








void readTSConfig()
{
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  DEBUG_PRINTLN("mounting FS...");

  if (SPIFFS.begin()) {
    DEBUG_PRINTLN("mounted file system");
    if (SPIFFS.exists("/configv2.json")) {
      //file exists, reading and loading
      DEBUG_PRINTLN("reading config file");
      File configFile = SPIFFS.open("/configv2.json", "r");
      if (configFile) {
        DEBUG_PRINTLN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          DEBUG_PRINTLN("\nparsed json");
          strcpy(ts_auth, json["ts_auth"]);
          strcpy(ts_server, json["ts_server"]);
          strcpy(wifi_ssid, json["wifi_ssid"]);
          strcpy(wifi_password, json["wifi_password"]);
          eic1_ugain=json["eic1_ugain"];
          eic1_igain=json["eic1_igain"];
          eic1_CRC2=json["eic1_CRC2"];
          eic1_CRC1=json["eic1_CRC1"];

          eic2_ugain=json["eic2_ugain"];
          eic2_igain=json["eic2_igain"];
          eic2_CRC2=json["eic2_CRC2"];
          eic2_CRC1=json["eic2_CRC1"];

          
        } else {
          DEBUG_PRINTLN("failed to load json config");
        }
      } 
    } else {
       DEBUG_PRINTLN("No config present, saving defaults");
       saveTSConfig();
    }
  } else {
    DEBUG_PRINTLN("failed to mount FS");
  }
  //end read
}




void saveTSConfig()
{
  //save the custom parameters to FS

  DEBUG_PRINTLN("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  
  json["ts_auth"] = ts_auth;
  json["ts_server"] = ts_server;
  json["wifi_ssid"] = wifi_ssid;
  json["wifi_password"] = wifi_password;


  json["eic2_ugain"]=eic2_ugain;
  json["eic2_igain"]=eic2_igain;
  json["eic2_CRC2"]=eic2_CRC2;
  json["eic2_CRC1"]=eic2_CRC1;


  json["eic1_ugain"]=eic1_ugain;
  json["eic1_igain"]=eic1_igain;
  json["eic1_CRC2"]=eic1_CRC2;
  json["eic1_CRC1"]=eic1_CRC1;

  
  File configFile = SPIFFS.open("/configv2.json", "w");
  if (!configFile) {
    DEBUG_PRINTLN("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
  DEBUG_PRINTLN("Resetting");
  ESP.reset();

}









void wifi_attemptToConnect(){

  if(wifi_ssid[0]==0){
    DEBUG_PRINTLN("No Network defined, starting AP");
    wifi_startSoftAP();
    return;
  }
  DEBUG_PRINTLN("Connecting to ");
  DEBUG_PRINTLN(wifi_ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  int loopcount=0;
  bool tryAgain=true;
  while (tryAgain) {
    delay(750);
    DEBUG_PRINTLN("Attemping connection..");
    loopcount++;
    
    if(WiFi.status() == WL_CONNECTED){
        tryAgain=false; //WiFi is connected, set the tryAgain flag to false so we exit to main loop
    }

    //Give up after 10 attempts and boot the softAP
    if(loopcount>10){
      DEBUG_PRINTLN("Unable to connect to configured network, starting AP");
      wifi_startSoftAP();
      tryAgain=false;   //Dont run this loop again
    }

    
  }


  DEBUG_PRINTLN("WiFi connected");  
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  
}


void wifi_startSoftAP(){
  String ap_name_str = "EMON_"+String(system_get_chip_id(),HEX);
  Serial.print("Configuring access point on SSID:");
  Serial.println(ap_name_str);
  WiFi.softAP(ap_name_str.c_str());  //No password
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  setupWebserver();
}


void http_handleReset(){
  server->send(200, "text/plain","Resetting...");
  DEBUG_PRINTLN("Resetting");
  ESP.reset();
}


void http_handleRoot() {

  String Wifi_SSID="var netArray = [";
  String Wifi_RSSI="var rSSI = [";
  int n = WiFi.scanNetworks();
  DEBUG_PRINTLN("scan done");
  if (n == 0)
    DEBUG_PRINTLN("no networks found");
  else
  {
    for (int i = 0; i < n; ++i)
    {
      Wifi_SSID+="\"";
      Wifi_SSID+=WiFi.SSID(i);
      Wifi_SSID+="\",";

      Wifi_RSSI+="\"";
      Wifi_RSSI+=WiFi.RSSI(i);
      Wifi_RSSI+="\",";
   }
    Wifi_SSID+="];";
    Wifi_RSSI+="];";
  }

  //Code minified using - https://github.com/LuckyMallari/html-minifier

  String webPage  = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>Config Editor";
  webPage += "</title></head><body><style type=\"text/css\">#content{text-align:center;wi";
  webPage += "dth:700px;margin-left:auto;margin-right:auto;}body{font-family:verdana,aria";
  webPage += "l,sans-serif;}table.gridtable{margin:0px auto;font-size:18px;color:#333333;";
  webPage += "border-width:1px;border-collapse:collapse;}table.gridtable th{border-width:";
  webPage += "1px;padding:8px;border-style:solid;background-color:#dedede;}table.gridtabl";
  webPage += "e td{border-width:1px;padding:8px;border-style:solid;background-color:#ffff";
  webPage += "ff;}</style><div id=\"content\"><h1>WhatNick Energymon Config</h1><form id=";
  webPage += "\"mainForm\" method=\"get\" action=\"/set\"><h2>Wifi Settings:</h2><table c";
  webPage += "lass=\"gridtable\"><tr><th>SSID</th><th>RSSI</th></tr><tbody ID=\"netTableB";
  webPage += "ody\"></tbody></table><br>SSID:<input id=\"txt_ssid\" type=\"text\" name=\"";
  webPage += "wifi_ssid\" />Password:<input type=\"text\" name=\"wifi_password\" /><br><h";
  webPage += "2>Other Options:</h2><br></form></div></body><script>";
  
  webPage +=Wifi_SSID;
  webPage +=Wifi_RSSI;
  webPage +="var optionals={\"";
  webPage +="eic2_CRC2\":";
  webPage +=eic2_CRC2;
  webPage +=",\"eic2_CRC1\":";
  webPage +=eic2_CRC1;
  webPage +=",\"eic2_igain\":";
  webPage +=eic2_igain;
  webPage +=",\"eic2_ugain\":";
  webPage +=eic2_ugain;
  
  webPage +=",\"eic1_CRC2\":";
  webPage +=eic1_CRC2;
  webPage +=",\"eic1_CRC1\":";
  webPage +=eic1_CRC1;
  webPage +=",\"eic1_igain\":";
  webPage +=eic1_igain;
  webPage +=",\"eic1_ugain\":";
  webPage +=eic1_ugain;
  
  webPage +="};";
  
  webPage += " var arrayLength=netArray.length;var tblBody=document.getElementById(\"netT";
  webPage += "ableBody\");var tableText=\"\";for(var i=0;i<arrayLength;i++){var text=\"<t";
  webPage += "r><td><a id=\"+i+\" onClick='reply_click(this.id)' href=#>\"+netArray[i]+\"";
  webPage += "</a></td><td>\"+rSSI[i]+\"</td></tr>\";tableText=tableText+text;}tblBody.in";
  webPage += "nerHTML=tableText;function reply_click(clicked_id){document.getElementById(";
  webPage += "\"txt_ssid\").value=netArray[clicked_id];}var form=document.getElementById(";
  webPage += "\"mainForm\");for(var k in optionals){if(optionals.hasOwnProperty(k)){newop";
  webPage += "tion=k+ \"<input type=text name=\"+k+\" value='\"+optionals[k]+\"'><br>\";f";
  webPage += "orm.insertAdjacentHTML('beforeend',newoption);}}form.insertAdjacentHTML(\"b";
  webPage += "eforeend\",\"<br><input value=Save type=submit>\");form.insertAdjacentHTML(";
  webPage += "\"beforeend\",\"<br><a href='/update'>Click here to upload new firmware</a>";
  webPage += "\");</script></html>";
  server->send(200, "text/html", webPage);
    
}

  
void http_handleSet(){
  if(strlen(server->arg("ts_auth").c_str())){
      strlcpy(ts_auth, server->arg("ts_auth").c_str(), sizeof(ts_auth));  
      Serial.print("Setting ts_auth to:");
      Serial.println(ts_auth);
    }
    
    if(strlen(server->arg("wifi_ssid").c_str())){
      strlcpy(wifi_ssid, server->arg("wifi_ssid").c_str(), sizeof(wifi_ssid));  
      Serial.print("Setting wifi_ssid to:");
      Serial.println(wifi_ssid);
    } 

    if(strlen(server->arg("wifi_password").c_str())){
      strlcpy(wifi_password, server->arg("wifi_password").c_str(), sizeof(wifi_password));  
      Serial.print("Setting wifi_password to:");
      Serial.println(wifi_password);
    } 


    if(strlen(server->arg("eic2_ugain").c_str())){
       eic2_ugain = atoi(server->arg("eic2_ugain").c_str());
       Serial.print("Setting eic2_ugain to:");
       Serial.println(eic2_ugain);
    }
    if(strlen(server->arg("eic2_igain").c_str())){
       eic2_igain = atoi(server->arg("eic2_igain").c_str());
       Serial.print("Setting eic2_igain to:");
       Serial.println(eic2_igain);
    }
    if(strlen(server->arg("eic2_CRC1").c_str())){
       eic2_CRC1 = atoi(server->arg("eic2_CRC1").c_str());
       Serial.print("Setting eic2_CRC1 to:");
       Serial.println(eic2_CRC1);
    }
    if(strlen(server->arg("eic2_CRC2").c_str())){
       eic2_CRC2 = atoi(server->arg("eic2_CRC2").c_str());
       Serial.print("Setting eic2_CRC2 to:");
       Serial.println(eic2_CRC2);
    }            

  
    if(strlen(server->arg("eic1_ugain").c_str())){
       eic1_ugain = atoi(server->arg("eic1_ugain").c_str());
       Serial.print("Setting eic1_ugain to:");
       Serial.println(eic1_ugain);
    }
    if(strlen(server->arg("eic1_igain").c_str())){
       eic1_igain = atoi(server->arg("eic1_igain").c_str());
       Serial.print("Setting eic1_igain to:");
       Serial.println(eic1_igain);
    }
    if(strlen(server->arg("eic1_CRC1").c_str())){
       eic1_CRC1 = atoi(server->arg("eic1_CRC1").c_str());
       Serial.print("Setting eic1_CRC1 to:");
       Serial.println(eic1_CRC1);
    }
    if(strlen(server->arg("eic1_CRC2").c_str())){
       eic1_CRC2 = atoi(server->arg("eic1_CRC2").c_str());
       Serial.print("Setting eic1_CRC2 to:");
       Serial.println(eic1_CRC2);
    } 


    server->send(200, "text/plain","Saved settings..Rebooting");
    delay(100);
    saveTSConfig();
   
    
}

void http_handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}





//HTTP Section
void setupWebserver(){
  DEBUG_PRINTLN("Starting webserver...");
  
  //Setup the runtime webserver
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  server->on("/", http_handleRoot);
  
  server->on("/set", http_handleSet);
  
  server->on("/reset", http_handleReset);

  server->on("/update", http_handleUpdate);
  
  
  server->onNotFound(http_handleNotFound);
  
  http_setupUpdate();
  
  server->begin();
  Serial.println("HTTP server started");
 
}


void http_handleUpdate(){
  const char* serverIndex = "<form method='POST' action='/doupdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
   server->send(200, "text/html",serverIndex);
}
void http_setupUpdate(){
  server->on("/doupdate", HTTP_POST, [](){
      server->sendHeader("Connection", "close");
      server->sendHeader("Access-Control-Allow-Origin", "*");
      server->send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
    },[](){
      HTTPUpload& upload = server->upload();
      if(upload.status == UPLOAD_FILE_START){
        Serial.setDebugOutput(true);
//        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if(Update.end(true)){ //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
}








void setupMetering(){
   /*Initialise the ATM90E26 + SPI port */
  eic1.SetLGain(0x240b);
  eic1.SetUGain(eic1_ugain);
  eic1.SetIGain(eic1_igain);
  eic1.SetCRC1(eic1_CRC1);
  eic1.SetCRC2(eic1_CRC2);
  int stat1 = eic1.GetMeterStatus();
  DEBUG_PRINT("Meter 1 Status:");
  DEBUG_PRINTLN(stat1);
  eic1.InitEnergyIC();
  //Hack meter 1 needs to initialized twice some SPI bus startup bug on ESP8266.
  delay(100);
  eic1.InitEnergyIC();

  eic2.SetLGain(0x240b);
  eic2.SetUGain(eic2_ugain);
  eic2.SetIGain(eic2_igain);
  eic2.SetCRC1(eic2_CRC1);
  eic2.SetCRC2(eic2_CRC2);
  int stat2 = eic1.GetMeterStatus();
  DEBUG_PRINT("Meter 2 Status:");
  DEBUG_PRINTLN(stat2);
  eic2.InitEnergyIC();
  delay(1000);
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
