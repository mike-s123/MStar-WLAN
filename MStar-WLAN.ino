 /*
 * MStar-WLAN 2020 mjs
 * 
 * ESP32 or ESP8266
 *   
 *  ESP32 - WROVER-B, custom partitions (WROVER 16 MB w/13M FS with modified files)
 *   lwIP: v2 higher bandwidth
 *   vTables: IRAM
 *   240 MHz
 *   
 *  ESP32 - WROOM (4 MB), 
 *    -ESP32 Dev Module
 *    -Default 4 MB w/SPIFFS (1.2 MB APP/1.5MB SPIFFS)
 *    or
 *    -Minimal (1.3 MB APP/700kB SPIFFS)
 *  
 *  ESP8266 - DEPRECATED, NO LONGER WORKING (WiFi scan broke with change to asynch webserver)
 *    Lolin D1 Mini Pro (deprecated - WiFi scanning doesn't work since 
 *    changing to asynch webserver - probably a memory shortage issue. #ifdefs are still here
 *    if someone wants to work on it. Future direction is ESP32.)
 *   -Upload: 921600
 *   -CPU: 160MHz
 *   -Flash: 16MB (FS:14MB...)
 *   -Debug: disabled
 *   -IwIP: v2 Higher bandwidth
 *   -vtables: IRAM

 *   Original work, License CC BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 *   All rights reserved.
 *   some parts subject to other licenses as noted.
 *   
 *   Using Arduino IDE 1.8.10, ESP8266 Arduino 2.6.2, ESP32 Arduino 1.0.4
 */

using namespace std; 
#define SOFTWARE_VERSION "v2.201022"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "ESP8266 support gone. Keep RTC in UTC. Dynamic updates of /status page.<br>\
                     Some changes for small flash. Change to ArduinoJSON 6, using PS_RAM.<br/>\
                     Allow WLAN and security settings."

#define DEBUG_ON 1               // enable debugging output. If defined, debug_level can be changed during runtime.
                                  // 0 off, 1 least detail, 8 most detail, 9 includes passwords

#ifdef DEBUG_ON
  #define BAUD_LOGGER 115200        // for software serial logging out "old" pins
                                    // because we're swapping the UART to new ones
  #ifdef ARDUINO_ARCH_ESP8266
    #define DEBUG_ESP_PORT logger
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    #define DEBUG_ESP_PORT Serial
  #endif
  
  //#define DEBUG_ESP_HTTP_SERVER
  //#define DEBUG_ESP_CORE
  //#define EZT_DEBUG DEBUG           // for EZTime
#endif

/*
 * If we define WIFI_MODE_AP_STA, it will both be an AP and try to connect as a station.
 * If not defined, we're a station if able to connect to a configured WLAN, otherwise
 * we turn into an AP. When an AP, if there are WLANs configured, it will try to connect
 * every few minutes if there is no client connected.
 */
#define WIFI_MODE_AP_STA                // define to run AP while also connected as station

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
//  #include <ESP8266WebServer.h>
//#include <ESP8266WebServerSecure.h>
//#include <ESP8266WebServerSecureAxTLS.h>
//#include <ESP8266WebServerSecureBearSSL.h>
#include <ESP8266mDNS.h>  //not working, comment out to not use
  /* This will compile in OTA support - need at least 1 MB for OTA updates.
     we check available space at runtime before allowing it.
  */
//  #include <ESP8266HTTPUpdateServer.h>
  #include <Updater.h>
  #include "ESPAsyncTCP.h"  //https://github.com/me-no-dev/ESPAsyncTCP
#endif

#include "ESPAsyncWebServer.h" 

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <WiFiMulti.h>
  //#include <WebServer.h>
  #include <AsyncTCP.h>
  #include "ESPAsyncWebServer.h"
  #include <ESPmDNS.h>
  #include <Update.h>
  #include <FS.h>
  #define FS_SPIFFS
  #define FILESYSTEM SPIFFS
  #define FS_TYPE "SPIFFS"
  #include <SPIFFS.h>
  #include <SD.h>
  #include <SPI.h>
  #include <sys/time.h>
//  #include "SdFat.h"
  #include <HardwareSerial.h>
  File logFile;                         // platform log file
  String logFileName;
  File ctl_logFile;                     // controller log file
  #define CTL_LOGFILE "/ctl.log"        // default, must start with /
  String ctlLogFileName = CTL_LOGFILE;
  #ifdef EZT_DEBUG
    File ezt_logFile;                   // ez-time log file
    #define EZT_LOGFILE "/eztime.log"   // must start with /
  #endif
#endif

#include <ArduinoJson.h>   // Benoit Blanchon 6.15.2, via IDE
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

//---------------------------
// definitions
//---------------------------

// hardware
#define PS_RAM      // PS-RAM available (WROVER-B)

//security
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "setup"
#define UPDATE_USERNAME "admin"
#define UPDATE_PASSWORD "update"
#define UPDATE_PATH "/update"
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


#define HOSTNAME "MStarW"  // needs to be short - esp32 only does 12 char hostnames!

#define JSON_VERSION "1.0"                // changes with api changes
#define JSON_VERSION_MIN "1.0"            // changes when backward compatibility is broken

// GPIO 2 for blue led on ESP-12E, GPIO 16 (or LED_BUILTIN, aka D0) for blue led on NodeMCU
// GPIO 2 (D4) for Wemos D1 mini
// GPIO 2 for WROVER-B board
#define WLAN_PIN 2

#define EEPROM_SIZE 1024  // ESP "eeprom"
#define EEPROM_SIG "mjs!"
/*
 * "EEPROM" on ESP
 * 0-127   (4x32) WLAN SSID
 * 128-255 (4x32) WLAN password
 * 256-271 (16) Controller model
 * 272-303 (32) ntp server
 * 304-305 (2)  ntp poll interval (uint_t 16)
 * 306-369 (64) ntp POSIX timestring
 * 370-401 (32) AP SSID
 * 402-433 (32) AP PSK
 * 434-449 (16) Admin name
 * 450-465 (16) Admin password
 * 466-481 (16) Upgrade name
 * 482-497 (16) Upgrade password
 * 498-513 (16) JSON password
 * 514-517 (4) Serial number
 * 514-1019  unused    
 * 1020-1023 (4)  Valid signature (EEPROM_SIG)
 */
#define eeWLANSSID 0
#define eeWLANPASS 128
#define eeModel 256
#define eeNtpServer 272
#define eeNtpPoll 304
#define eeNtpTZ 306
#define eeAPSSID 370
#define eeAPPSK 402
#define eeAdminName 434
#define eeAdminPass 450
#define eeUpgradeName 466
#define eeUpgradePass 482
#define eeJsonPass 498
#define eeSerialNum

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
string web_username = WEB_USERNAME;
string web_password = WEB_PASSWORD;
string json_password = JSON_PASSWORD;
string json_version = JSON_VERSION;
string json_version_min = JSON_VERSION_MIN;
string ap_ssid = AP_SSID;
string ap_password = AP_PSK;
string ap_SSID = AP_SSID;
string root_username = UPDATE_USERNAME;
string root_password = UPDATE_PASSWORD;
string serialNumber = SERIAL_NUMBER;
const char *update_path = UPDATE_PATH;
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
boolean mytz2skip = false; //testing
uint32_t mytz2skipMillis;

// used for flashing the WLAN status LED
int blinkOnTime = 1000;
int blinkRepeatTime = 2000;
unsigned long lastMillis = 0;
unsigned long lastWLANtry;      // when we last tried to connected (or tried) to an AP
int mbAddr = mbusSlave;
String model = MODEL;
String fullModel = MODEL;
String ctlSerialNum = "00000000";
byte mac[6];
String my_MAC;
String my_hostname;
File fsUploadFile;              // a File object to temporarily store the received file

String referrer; 
ModbusMaster node;

#ifdef ARDUINO_ARCH_ESP8266
  ESP8266WiFiMulti wifiMulti;
//  ESP8266WebServer server(80);
//  ESP8266HTTPUpdateServer httpUpdater;
#endif
WiFiServer modbusTCP(502);
WiFiClient modbusClient;

AsyncWebServer server(80);

#ifdef ARDUINO_ARCH_ESP32
  WiFiMulti wifiMulti;
//  WebServer server(80);
  HardwareSerial mbSerial(1);
#endif

//order here is important
#include "TimeStuff.h"
#include "Utility.h"            // utility functions
#include "WLAN.h"               // wireless stuff
//#include "edit.h"               // ACE editor
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
    attachSDCardIRQ();
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
//      httpUpdater.setup(&server, update_path, root_username, root_password);
    #endif
  }

  EEPROM.begin(EEPROM_SIZE);
  delay(10);
  getEeConfig(); // load config from eeprom
  getWLANsFromEEPROM();
  setupWLAN();
  setupClocks();
/*  
  #ifndef WIFI_MODE_AP_STA
    if (!wlanConnected) {             // If can't connect in STA mode, switch to AP mode
      startAP(ap_ssid, ap_password);
    }
  #endif
  
  IPAddress apIP = WiFi.softAPIP();
  IPAddress myIP = WiFi.localIP();

  if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) {
    debugMsg(F("AP IP address:"),1);
    debugMsgln(formatIPAsString(apIP),1);
  }
  if (wlanConnected) {
    debugMsg(F("WLAN IP address:"),1);
    debugMsgln(formatIPAsString(myIP),1);
    debugMsgln("Connected to:" + String(WiFi.SSID()),1);
  }
*/
  setupModbus();
  server.begin();
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
  #ifdef ARDUINO_ARCH_ESP32
    refreshCtlLogFile();
  #endif

  getRtcTime();
  WiFi.setTxPower(WIFI_POWER_19dBm); // 15 is about 1/3 the power of the default 19.5 dBm

  setEvents(); // setup regular events.
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
/*  if (!mytz2skip || millis() > mytz2skipMillis + 180000) {   // don't do events around 0200 local, test
    unsigned long int tempmillis=0;
    if (mytz2skip) tempmillis = millis();
    events(); // for EZTime
    if (tempmillis > 0) {
      debugMsgln("event millis:"+String(millis()-tempmillis),1);
      if (logFile) logFile.flush();
      WiFi.setTxPower(WIFI_POWER_15dBm);
      mytz2skip = false;
    }
  }
*/
  
  checkNtp();
//  server.handleClient();

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
  } else {                                              // no connections, every 10 sec
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
