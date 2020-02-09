/*
 * MStar-WLAN 2020 mjs
 * 
 * ESP8266 (working) or ESP32
 *   ESP8266 - Lolin D1 Mini Pro
 *   -Upload: 921600
 *   -CPU: 160MHz
 *   -Flash: 16MB (FS:14MB...)
 *   -Debug: disabled
 *   -IwIP: v2 Higher bandwidth
 *   -vtables: IRAM
 *   
 *   ESP32 - WROVER-B, custom partitions (WROVER 16 MB w/13M FS with modified files)
 *   lwIP: v2 higher bandwidth
 *   vTables: IRAM
 *   
 *   Original work, License CC BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 *   All rights reserved.
 *   some parts subject to other licenses as noted.
 *   
 *   Using Arduino IDE 1.8.10, ESP8266 Arduino 2.6.2, ESP32 Arduino 1.0.4
 */

#define SOFTWARE_VERSION "v1.200207"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "Refactor for different controller families. ESP32 working.<br>\
                     wifiMulti (no GUI). WLAN robustness. WIFI_AP_STA support.<br>\
                     Time support. Auto adjust RTC speed. SD card support (ESP32).<br>\
                     Platform logging to SD card. Change for /sd/ wildcard. Controller<br>\
                     logging to SD Card."

#define DEBUG_ON 2                // enable debugging output. If defined, debug_level can be changed during runtime.
                                  // 0 off, 1 least detail, 8 most detail, 9 includes passwords
//#define DEBUG_JS                   // ifdef, overrides servestatic to avoid caching of local.js
//#define DEBUG_CSS                  // ditto, for css

#ifdef DEBUG_ON
  #define BAUD_LOGGER 115200        // for software serial logging out "old" pins
                                    // because we're swapping the UART to new ones
  #define DEBUG_ESP_PORT logger
  //#define DEBUG_ESP_HTTP_SERVER
  //#define DEBUG_ESP_CORE
  //#define EZT_DEBUG DEBUG           // for EZTime
#endif

//#define STATIC_IP

#include <string>
#include <sstream>
#include <EEPROM.h>
#include <FS.h>
#include <Wire.h>

//#include <BearSSLHelpers.h>
//#include <CertStoreBearSSL.h>
#ifdef ARDUINO_ARCH_ESP8266
  #include <SoftwareSerial.h>
  ADC_MODE(ADC_VCC);
  #ifdef DEBUG_ON
      SoftwareSerial logger(3, 1); // RX, TX
  #else
//    SoftwareSerial* cSerial = nullptr;  
  #endif

//  #define FS_SPIFFS
  #define FS_LITTLEFS
  #ifdef FS_SPIFFS
    #define FILESYSTEM SPIFFS
    #define FS_TYPE "SPIFFS"
//    #include <SPIFFS.h>
  #endif
  #ifdef FS_LITTLEFS
    #define FILESYSTEM LittleFS
    #define FS_TYPE "LittleFS"
    #include <LittleFS.h>
  #endif
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiAP.h>
  #include <ESP8266WiFiGeneric.h>
  #include <ESP8266WiFiMulti.h>
  #include <ESP8266WiFiScan.h>
  #include <ESP8266WiFiSTA.h>
  #include <ESP8266WiFiType.h>
  #include <ESP8266WebServer.h>
//#include <ESP8266WebServerSecure.h>
//#include <ESP8266WebServerSecureAxTLS.h>
//#include <ESP8266WebServerSecureBearSSL.h>
#include <ESP8266mDNS.h>  //not working, comment out to not use
  /* This will compile in OTA support - need at least 1 MB for OTA updates.
     we check available space at runtime before allowing it.
  */
  #include <ESP8266HTTPUpdateServer.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <WiFiMulti.h>
  #include <WebServer.h>
  //#include "ESPAsyncWebServer.h"
  #include <ESPmDNS.h>
  #include <Update.h>
  #include <FS.h>
  #define FS_SPIFFS
  #define FILESYSTEM SPIFFS
  #define FS_TYPE "SPIFFS"
  #include <SPIFFS.h>
  #include <SD.h>
  #include <SPI.h>
//  #include "SdFat.h"
  #include <HardwareSerial.h>
  File logFile;                         // platform log file
  String logFileName;
  File ctl_logFile;                     // controller log file
  #define CTL_LOGFILE "/controller.log" // must start with /
  #ifdef EZT_DEBUG
    File ezt_logFile;                   // ez-time log file
    #define EZT_LOGFILE "/eztime.log"   // must start with /
  #endif
#endif

#include <ArduinoJson.h>   // Benoit Blanchon 5.13.4, via IDE
#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
//#include <WiFiClientSecureAxTLS.h>
//#include <WiFiClientSecureBearSSL.h>

#include <WiFiServer.h>
//#include <WiFiServerSecure.h>
//#include <WiFiServerSecureAxTLS.h>
//#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>
#include <ModbusMaster.h> //Doc Walker 2.0.1, via IDE

#include "TimeStuff.h"
//---------------------------
// definitions
//---------------------------

//security
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "setup"
#define UPDATE_USERNAME "admin"
#define UPDATE_PASSWORD "update"
#define UPDATE_PATH "/updateme"
#define JSON_PASSWORD "imsure"
//#define nosec
#ifdef nosec
  #define WEB_USERNAME ""
  #define WEB_PASSWORD ""
  #define UPDATE_USERNAME ""
  #define UPDATE_PASSWORD ""
#endif

//wlan
#define AP_SSID "MStar"
#define AP_SSID_UNIQ                    // define to generate a unique SSID e.g. MStar-123456
// password, if used, must be 8-32 chars
#define AP_PSK  "morningstar"

/*
 * If we define WIFI_MODE_AP_STA, it will both be an AP and try to connect as a station.
 * If not defined, we're a station if able to connect to a configured WLAN, otherwise
 * we turn into an AP. When an AP, if there are WLANs configured, it will try to connect
 * every few minutes if there is no client connected.
 */
//#define WIFI_MODE_AP_STA                // define to run AP while also connected as station

#define HOSTNAME "MStarW"  // needs to be short - esp32 only does 12 char hostnames!

#define JSON_VERSION "1.0"                // changes with api changes
#define JSON_VERSION_MIN "1.0"            // changes when backward compatibility is broken

// GPIO 2 for blue led on ESP-12E, GPIO 16 (or LED_BUILTIN, aka D0) for blue led on NodeMCU
// GPIO 2 (D4) for Wemos D1 mini
// GPIO 2 for WROVER-B board
#define WLAN_PIN 2

#define EEPROM_SIZE 512  // ESP "eeprom"
#define EEPROM_SIG "mjs!"
/*
 * "EEPROM" on ESP
 * 0-127   (4x32) WLAN SSID
 * 128-255 (4x32) WLAN password
 * 256-272 (16) Controller model
 * 272-303 (32) ntp server
 * 304-305 (2)  ntp poll interval (uint_t 16)
 * 306-337 (32) ntp POSIX timestring
 * 508-511 (4)  Valid signature (EEPROM_SIG)
 */
#define eeWLANSSID 0
#define eeWLANPASS 128
#define eeModel 256
#define eeNtpServer 272
#define eeNtpPoll 304
#define eeNtpTZ 306

#ifdef ARDUINO_ARCH_ESP8266
  #define RX_ENABLE_PIN 12  // GPIO 12 (D6) on Wemos
  #define SDA_PIN 4         // GPIO 4 (D2) I2C SDA
  #define SCL_PIN 5         // GPIO 5 (D1) I2C SCL
  #define SELF_RST_PIN 16   // GPIO 16 (D0) self-reset
//  #define I2C_SDA_RESET     // No pin available on 8266
#endif
#ifdef ARDUINO_ARCH_ESP32
  #define RX_ENABLE_PIN 25  // RxEna to IO25, pin 10
  #define RX_PIN 27         // RxD to IO27, pin 12
  #define TX_PIN 4          // Txd to IO4, pin 26
  #define SDA_PIN 15        // GPIO 15  I2C SDA
  #define SCL_PIN 13        // GPIO 13  I2C SCL
  #define SPI_SCLK 18       // these are default pins for VSPI
  #define SPI_MISO 19       // just here for documentation
  #define SPI_MOSI 23
  #define SD_CARD0_CS 5        // CS for SDCard0
  #define SD_CARD1_CS 32 // for SDCard1, connected to header
  #define SD_CARD_TO_USE 0  // Which SD card to use
  #define SD_DETECT 26    // SD card inserted, only for SDCard0
  #define I2C_SDA_RESET 21 // https://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html
  #define SD_CARD_LOG true //log to SD card
  bool sd_card_log = SD_CARD_LOG;
  bool sd_card_available = false;
  volatile bool sd_card_changed = false;
  bool needLogTime = true;  // whether we need to print time to the log
#endif
// MBus slave id of controller
#define mbusSlave 1

// controller type
#define MODEL "PS-PWM"

// modbus items
#define REG_ROWS 126
#define COIL_ROWS 20

// ----------------------------------------------------------------------------------------------------
// ------------------------------------------  Globals (mostly)  --------------------------------------
// ----------------------------------------------------------------------------------------------------

#ifdef DEBUG_ON
  int debug_level = DEBUG_ON;
#else
  int debug_level = 0;
#endif
const char *web_username = WEB_USERNAME;
const char *web_password = WEB_PASSWORD;
const char *json_password = JSON_PASSWORD;
const char *json_version = JSON_VERSION;
const char *json_version_min = JSON_VERSION_MIN;
const char *ap_ssid = AP_SSID;
const char *ap_password = AP_PSK;
std::string ap_SSID(AP_SSID);      //this is butt-ugly. see below. Someone else can do better.

const char *update_path = UPDATE_PATH;
const char *update_username = UPDATE_USERNAME;
const char *update_password = UPDATE_PASSWORD;
const char *serialNumber = SERIAL_NUMBER;

const char *fs_type = FS_TYPE;

String esid[4];
String epass[4];
boolean wlanConnected = false;
int wlan_count=0;             // holds how many WLANs configured in EEPROM
boolean wlanRead = false;     // if we've read SSID/PSKs from EEPROM
boolean wlanSet = false;      // if we've added them to wifiMulti
boolean wlansAdded = false;   // if we've added WLANs to wifiMulti
boolean largeFlash = false;
boolean mbActive = false;    // whether we're using mbus
boolean wlanLedState = true;
boolean noController = true;
// used for flashing the WLAN status LED
int blinkOnTime = 1000;
int blinkRepeatTime = 2000;
unsigned long lastMillis = 0;
unsigned long lastWLANtry;      // when we last tried to connected (or tried) to an AP
int mbAddr = mbusSlave;
String model = MODEL;
String fullModel = MODEL;
byte mac[6];
String my_MAC;
String my_hostname;
File fsUploadFile;              // a File object to temporarily store the received file

String referrer; 
ModbusMaster node;

#ifdef ARDUINO_ARCH_ESP8266
  ESP8266WiFiMulti wifiMulti;
  ESP8266WebServer server(80);
  ESP8266HTTPUpdateServer httpUpdater;
#endif
WiFiServer modbusTCP(502);
WiFiClient modbusClient;
  
#ifdef ARDUINO_ARCH_ESP32
  WiFiMulti wifiMulti;
//  AsyncWebServer server(80);
  WebServer server(80);
  HardwareSerial mbSerial(1);
#endif

//order here is important
#include "Utility.h"            // utility functions
#include "WLAN.h"               // wireless stuff
#include "edit.h"               // ACE editor
#include "MBus.h"               // Handle MODBUS and datatype conversion
#include "ControllerFiles.h"    // Read and parse controller .csv files
#include "HTML.h"               // Common HTML stuff
#include "RestPage.h"           // REST API
#include "PS.h"                 // status, charge and other pages for Prostar models
#include "WebPages.h"           // stuff to serve content
#include "Setups.h"
#include "Web.h"                // starts up web server

void setup() {
  #ifdef ARDUINO_ARCH_ESP32
    pinMode(I2C_SDA_RESET ,INPUT);
    setupSDCard();
  #endif
  
  #ifdef ARDUINO_ARCH_ESP8266
    digitalWrite(SELF_RST_PIN, HIGH); // So board can do a hardware reset of itself
    pinMode(SELF_RST_PIN, OUTPUT);
  #endif
  
  WiFi.macAddress(mac);
  my_MAC =  String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);
  my_hostname = HOSTNAME + String("-") + my_MAC;
  #ifdef ARDUINO_ARCH_ESP32
    logFileName = "/"+ my_hostname + ".log";
  #endif
  setupRtcSQW();  // real time clock
  setupComms();   // serial port stuff
  setupFS();      // filesystems

  if ( ESP.getSketchSize() + 4096 < ESP.getFreeSketchSpace() ) {
    largeFlash = true;
  }
  if ( largeFlash ) {
    #ifdef ARDUINO_ARCH_ESP8266
      httpUpdater.setup(&server, update_path, update_username, update_password);
    #endif
  }

  EEPROM.begin(EEPROM_SIZE);
  delay(10);
  if (checkEEPROM() != EEPROM_SIG) { delay(10); }
  getWLANsFromEEPROM();
  setupWLAN();
  setupClocks();
  
  #ifndef WIFI_MODE_AP_STA
    if (!wlanConnected) {             // If can't connect in STA mode, switch to AP mode
      startAP(ap_ssid, ap_password);
    }
  #endif
  
  IPAddress apIP = WiFi.softAPIP();
  IPAddress myIP = WiFi.localIP();
  
  debugMsg(F("AP IP address:"),1);
  debugMsgln(formatIPAsString(apIP),1);
  if (wlanConnected) {
    debugMsg(F("WLAN IP address:"),1);
    debugMsgln(formatIPAsString(myIP),1);
    debugMsgln("Connected to:" + String(WiFi.SSID()),1);
  }

  setupModbus();
  startWeb();

  modbusTCP.begin();

  if (!MDNS.begin("MStarWLAN")) {
    debugMsgln(F("Error setting up MDNS responder!"),1);
  } else {
    MDNS.addService("http", "tcp", 80);
    debugMsgln(F("mDNS responder started"),1);
  }

  debugMsgln("Getting modbus info for " + model,1);
  getFile(model);

  getRtcTime();
  debugMsgln(F("Leaving setup()"),1);
} // setup()

void loop() {
  
  #ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtFeed();
  #endif

  #ifdef MDNS_H // ESP8266
    MDNS.update();
  #endif
  events(); // for EZTime
  checkNtp();
  server.handleClient();

  #ifdef ARDUINO_ARCH_ESP32
    if (sd_card_changed) changeSDCard();
  #endif
  
  #ifndef DEBUG_ON
    serialPassthrough();
  #endif

// This handles connections to TCP/502 for Modbus TCP from MSView
  if (!modbusClient.connected()) {
    modbusClient = modbusTCP.available();
  } else {
    mbusTCP();
  }

/*
 *  Blink the LED based on current connection state.
 */
 
  if (WiFi.softAPgetStationNum()) {                     // AP client connected, mostly on
    blinkOnTime = 275;       
    blinkRepeatTime = 300;
  } else if (WiFi.status() == WL_CONNECTED) {           // Connected as STA (or AP), flash 1/sec
    wlanConnected = true;
    blinkOnTime = 200;
    blinkRepeatTime = 1000;
  } else {                                              // no connections, every 3 sec
    blinkOnTime = 25;
    blinkRepeatTime = 10000;  
  }

  if (((millis() - lastMillis) > blinkRepeatTime) && wlanLedState) {
    lastMillis = millis();
    wlanLedState = false;
    setWlanLED(wlanLedState);
  } else if (((millis() - lastMillis) > blinkOnTime) && !wlanLedState) {
    wlanLedState = true;
    setWlanLED(wlanLedState);
  }
} // loop()
