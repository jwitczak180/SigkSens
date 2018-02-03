#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <ESP8266mDNS.h>        // Include the mDNS library
#include <ESP8266SSDP.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ArduinoJson.h>     //https://github.com/bblanchon/ArduinoJson

#include <string>

#include "config.h"
#include "FSConfig.h"
#include "i2c.h"
#include "mpu.h"
#include "mpu9250.h"
#include "sht30.h"
#include "oneWire.h"
#include "digitalIn.h"
#include "systemHz.h"
#include "configReset.h"
#include "webSocket.h"
#include "signalK.h"
#include "httpd.h"
#include "sigksens.h"


/*---------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------
Global Variables
-----------------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------*/

char myHostname[16];

uint16_t mainLoopCount = 0; //some stuff needs to run constantly, others not. so run some stuff only every X loops.

/*---------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------
General Setup
-----------------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------*/

void setupFromJson() {
  fromJson[(int)SensorType::local] =
    (fromJsonFunc)&(SystemHzSensorInfo::fromJson);

  fromJson[(int)SensorType::digitalIn] =
    (fromJsonFunc)&(DigitalInSensorInfo::fromJson);

  fromJson[(int)SensorType::oneWire] =
    (fromJsonFunc)&(OneWireSensorInfo::fromJson);

  fromJson[(int)SensorType::sht30] =
    (fromJsonFunc)&(SHT30SensorInfo::fromJson);

  fromJson[(int)SensorType::mpu925x] =
    (fromJsonFunc)&(MPU9250SensorInfo::fromJson);
}


void setupWifi() {
  WiFiManager wifiManager;
 
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  WiFiManagerParameter custom_hostname("myHostname", "Set Hostname", myHostname, 16);

  wifiManager.addParameter(&custom_hostname);
  
  wifiManager.autoConnect("Unconfigured Sensor");
  Serial.println("Connected to Wifi!");

  // Save config if needed
  if (shouldSaveConfig) {
    strcpy(myHostname, custom_hostname.getValue());
    saveConfig();
  }
}


void setupDiscovery() {
  if (!MDNS.begin(myHostname)) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.print ("mDNS responder started at ");
    Serial.print (myHostname);
    Serial.println("");
  }
  MDNS.addService("http", "tcp", 80);
  
  Serial.printf("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName(myHostname);
    SSDP.setSerialNumber("12345");
    SSDP.setURL("index.html");
    SSDP.setModelName("WifiSensorNode");
    SSDP.setModelNumber("12345");
    SSDP.setModelURL("http://www.signalk.org");
    SSDP.setManufacturer("SigK");
    SSDP.setManufacturerURL("http://www.signalk.org");
    SSDP.setDeviceType("upnp:rootdevice");
    SSDP.begin();
}


void setup() {
  bool need_save = false;
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);

  setupFromJson();

  setupFS();

  setupWifi();
  loadConfig();
  setupDiscovery();
  setupHTTP();
  setupWebSocket();
  setupSignalK();

  setupConfigReset();
  setup1Wire(need_save);
  setupI2C(need_save);
  setupDigitalIn(need_save);
  setupSystemHz(need_save);
  
  if (need_save) {
    saveConfig();
  }
  
  Serial.printf("Ready!\n");
}

/*---------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------
Main Loop!
-----------------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------*/


void loop() {
  bool need_save = false;
  //device mgmt
  yield();           
  
  //Stuff here run's all the time
  handleSystemHz();
  handleI2C();

  mainLoopCount++;
  
  //Stuff that runs  once every 1000 loops. (still many many times/sec)
  if (mainLoopCount > 1000) {
      handleI2C_slow();
      handle1Wire(need_save);
      if (need_save) {
        saveConfig();
      }
  
      handleWebSocket();
      handleSignalK();
      handleDigitalIn();
      httpServer.handleClient(); //http client
      
      handleConfigReset(); 
      mainLoopCount = 0;
  }
}