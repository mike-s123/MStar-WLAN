/*
 * MStar-WLAN 2019 mjs
 * 
 * ESP8266 (working) or ESP32 (work in progress)
 *   ESP8266 - Lolin D1 Mini Pro
 *   -Upload: 921600
 *   -CPU: 160MHz
 *   -Flash: 16MB (FS:14MB...)
 *   -Debug: disabled
 *   -IwIP: v2 Higher bandwidth
 *   -vtables: IRAM
 *   
 *   ESP32 - M5Stack-FIRE, Large FILESYSTEM (7 MB) (WROVER 16 MB w/13M FS with modified files)
 *   cpu: fastest
 *   flash: 2M+ FILESYSTEM
 *   lwIP: v2 higher bandwidth
 *   vTables: IRAM
 *   
 *   Original work, License CC BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 *   some parts subject to other licenses as noted.
 *   
 *   Using Arduino IDE 1.8.10, ESP8266 Arduino 2.6.2, ESP32 Arduino 1.0.4
 */

#define SOFTWARE_VERSION "v0.191218"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "Add more controller datatypes. Speed reading. Auto refresh.<br/>\
                     No mDNS. Refactor for different controller families. Work on ESP32.<br/>\
                     wifiMulti"

#define DEBUG_ON 4                // enable debugging output
                                  // 0 off, 1 least detail, 5 most detail, 6 includes passwords
                                  // 0 not working on ESP32 for now
#define BAUD_LOGGER 115200        // for software serial logging out "old" pins
                                  // because we're swapping the UART to new ones
#define DEBUG_ESP_PORT logger
//#define DEBUG_ESP_HTTP_SERVER
//#define DEBUG_ESP_CORE

#include <string>
#include <sstream>
#include <EEPROM.h>
#include <FS.h>

#include <Wire.h>
#include <DS3231.h>   // Andrew Wickert, et al 1.0.2, via IDE
#include <ZEeprom.h>  // Pierre Valleau 1.0.0, for EEPROM on DS3231 board, via IDE
//#include <BearSSLHelpers.h>
//#include <CertStoreBearSSL.h>
#ifdef ARDUINO_ARCH_ESP8266
  #include <SoftwareSerial.h>
  ADC_MODE(ADC_VCC);
  #if DEBUG_ON>0
      SoftwareSerial logger(3, 1); // RX, TX
  #else
    SoftwareSerial* cSerial = nullptr;  
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
//#include <ESP8266mDNS.h>  //not working, comment out to not use
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
  #include <HardwareSerial.h>
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
#define AP_SSID "MStar"
// password, if used, must be 8-32 chars
#define AP_PSK  "morningstar"
//#define AP_PSK  ""

//#define nosec
#ifdef nosec
  #define WEB_USERNAME ""
  #define WEB_PASSWORD ""
  #define UPDATE_USERNAME ""
  #define UPDATE_PASSWORD ""
#endif

#define HOSTNAME "MStarWLAN"

#define JSON_VERSION "1.0"               // changes with api changes
#define JSON_VERSION_MIN "1.0"           // changes when backward compatibility is broken

// GPIO16 (D0) on NodeMCU, 1 on ESP-1, but won't work on ESP-1 because of serial?
// GPIO 2 for blue led on ESP-12E, GPIO 16 (or BUILTIN_LED) for blue led on NodeMCU
// GPIO 2 (D4) for Wemos D1 mini
#define WIFI_PIN 2
//#define WIFI_PIN BUILTIN_LED

#define EEPROM_SIZE 512
#define EEPROM_SIG "mjs!"
/*
 * 
 * 0-127   4x32 WLAN SSID
 * 128-255 4x32 WLAN password
 * 256-272 16 Controller model
 * 508-511 (4)  Valid signature (EEPROM_SIG)
 */
#define CLK_EEPROM_SIZE 4096
#define CLK_EEPROM_SIG "mjs!"
/*
 * 0    (1) Last set aging offset (so we have it if battery goes away)
 * 1-2  (2) Year
 * 3    (1) Month
 * 4    (1) Day
 * 5    (1) Hour
 * 6    (1) Minute
 * 7    (1) Second
 * 
 * 4092-4095 (4)  Valid signature (CLK_EEPROM_SIG)
 */
#define eeAge 0
#define eeYear 1
#define eeMonth 3
#define eeDay 4
#define eeHour 5
#define eeMinute 6
#define eeSecond 7
#define eeUnix 8

#ifdef ARDUINO_ARCH_ESP8266
  #define RX_ENABLE_PIN 12  // GPIO 12 (D6) on Wemos
  #define SDA_PIN 4         // GPIO 4 (D2) I2C SDA
  #define SCL_PIN 5         // GPIO 5 (D1) I2C SCL
  #define SELF_RST_PIN 16   // GPIO 16 (D0) self-reset
#endif
#define SQW_PIN 14        // GPIO 14 (D5) on both, SQW from DS3231
#ifdef ARDUINO_ARCH_ESP32
  #define RX_ENABLE_PIN 25  // RxEna to IO25, pin 10  (was IO23)
  #define RX_PIN 27         // RxD to IO27, pin 12
  #define TX_PIN 4          // Txd to IO4, pin 26
  #define SDA_PIN 15        // GPIO was 15  I2C SDA
  #define SCL_PIN 13        // GPIO was 13  I2C SCL
#endif
// MBus slave id
#define mbusSlave 1

// controller type
#define MODEL "PS-PWM"

// modbus items
#define REG_ROWS 126
#define COIL_ROWS 20


// ----------------------------------------------------------------------------------------------------
// ------------------------------------------  Globals (mostly)  --------------------------------------
// ----------------------------------------------------------------------------------------------------

const char *ap_ssid = AP_SSID;
const char *ap_password = AP_PSK;
const char *web_username = WEB_USERNAME;
const char *web_password = WEB_PASSWORD;
const char *json_password = JSON_PASSWORD;
const char *json_version = JSON_VERSION;
const char *json_version_min = JSON_VERSION_MIN;

const char *update_path = UPDATE_PATH;
const char *update_username = UPDATE_USERNAME;
const char *update_password = UPDATE_PASSWORD;
const char *serialNumber = SERIAL_NUMBER;

const char *fs_type = FS_TYPE;

String esid[4];
String epass[4];
boolean wlanConnected = false;
boolean largeFlash = false;
boolean mbActive = false;    // whether we're using mbus
boolean blueLedState = true;
boolean noController = true;
// used for flashing the blue LED
int blinkOnTime = 1000;
int blinkTopTime = 2000;
unsigned long lastMillis = 0;
int mbAddr = mbusSlave;
String model = MODEL;
String fullModel = MODEL;
String my_hostname;
File fsUploadFile;              // a File object to temporarily store the received file

String referrer; 
IPAddress apIP(192, 168, 99, 1);
ModbusMaster node;
DS3231 Clock;
ZEeprom * clk_eeprom;
#define DS3231_I2C 0x68
#define AT24Cxx_BASE_ADDR 0x57 // ZS-042/HW-84 modules have pullups on A0-A2
#define AGE_OFFSET -24       // aging offset for ds3231, higher is slower
                            // ~0.1 ppm per (~9 ms/day, 0.26 sec/month), higher is slower, 
                            // 11.6 ppm is ~ 1 sec/day
bool useRTC = false;
bool twentyFourHourClock = true;
bool Century=false;
bool h12;
bool PM;


#ifdef ARDUINO_ARCH_ESP8266
  ESP8266WiFiMulti wifiMulti;
  ESP8266WebServer server(80);
  ESP8266HTTPUpdateServer httpUpdater;
#endif
WiFiServer mbTCP(502);
WiFiClient mbClient;
  
#ifdef ARDUINO_ARCH_ESP32
  WiFiMulti wifiMulti;
//  AsyncWebServer server(80);
  WebServer server(80);
  HardwareSerial mbSerial(1);
#endif

int Year = 2019;
byte Month = 4, Day = 1, Weekday = 1, Hour = 0, Minute = 0, Second = 0;  // April fool's

//order here is important
#include "Utility.h"            // utility functions
#include "edit.h"               // ACE editor
#include "MBus.h"               // Handle MODBUS and datatype conversion
#include "ControllerFiles.h"    // Read and parse controller .csv files
#include "HTML.h"               // Common HTML stuff
#include "RestPage.h"           // REST API
#include "PS.h"                 // status, charge and other pages for Prostar models
#include "WebPages.h"           // stuff to serve content



void setup() {
  #ifdef ARDUINO_ARCH_ESP8266
    digitalWrite(SELF_RST_PIN, HIGH);
    pinMode(SELF_RST_PIN, OUTPUT);
  #endif
  pinMode(SQW_PIN, INPUT);            // From DS3231
  digitalWrite(SQW_PIN, HIGH);        // internal pullup
  #ifdef ARDUINO_ARCH_ESP8266
    Serial.begin(9600, SERIAL_8N2);
    U0C0 = BIT(UCRXI) | BIT(UCTXI) | BIT(UCBN) | BIT(UCBN + 1) | BIT(UCSBN) | BIT(UCSBN + 1); // Inverse RX & TX, 8N2
    /*
     * Here we swap to the alternate UART pins, so they're now GPIO15(D8,TX) and GPIO13(D7,RX)
     * This is so we can use the more reliable hardware UART to talk to MODBUS
     * We'll then use softwareserial to send debug messages out the old UART
     * pins (which go to the USB/serial chip).
     */
    Serial.swap();  //
    #if DEBUG_ON>0
      delay(100);
      setupDebug();
      delay(100);
      debugMsgContinue(F("Debug on, level "));
      debugMsg(String(DEBUG_ON));
    #else
      cSerial = new SoftwareSerial(3, 1);
      setupPassthru();
      delay(100);
      cSerial->println(F("cSerial setup"));
    #endif
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    /* remap UART1 
     *  
     *  pinMatrixOutAttach(uint8_t pin, uint8_t function, bool invertOut, bool invertEnable);
     *  pinMatrixInAttach(uint8_t pin, uint8_t signal, bool inverted);
    */
    setupDebug();
    debugMsgContinue(F("Debug on, level "));
    debugMsg(String(DEBUG_ON));
    pinMode(RX_PIN, INPUT);
    delay(100);
    mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);    
//    pinMatrixOutAttach(TX_PIN, U1TXD_OUT_IDX, true, false);
//    pinMatrixInAttach(RX_PIN, U1RXD_IN_IDX, true);
  #endif

    Wire.begin(SDA_PIN, SCL_PIN);
    if (Wire.requestFrom(DS3231_I2C, 2)) {
      useRTC = true;
    } else {
      #if DEBUG_ON>0
        debugMsg(F("No RTC found"));
      #endif      
    }
    if (useRTC) {
      delay(10);
      if (!Clock.oscillatorCheck()) {  // check for Oscillator Stopped Flag (!good RTC)
        #if DEBUG_ON>0
          debugMsg(F("Initializing RTC"));
        #endif
        Clock.enableOscillator(true, true, 0);  // Ena osc, sqw on batt, 1 Hz
        delay(2);
        setRTC();   // Clears OSF flag
        delay(2);
        if (!Clock.oscillatorCheck()) {
          useRTC = false;
          #if DEBUG_ON>0
            debugMsg(F("RTC check failed"));  // probably no RTC
          #endif
        }
      }
    }  
    // Initialize EEPROM library.
    clk_eeprom= new ZEeprom();
    clk_eeprom->begin(Wire,AT24Cxx_BASE_ADDR,AT24C32);


  pinMode(RX_ENABLE_PIN, OUTPUT);
  rxEnable(false);
  pinMode(WIFI_PIN, OUTPUT);  // LED

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
  #if DEBUG_ON>0
    debugMsgContinue(F("esid:"));
    debugMsg(esid[0]);
  #endif
  #if DEBUG_ON>5
    debugMsg("epass:"+epass[0]);  
  #endif

/*
 * WiFi setup
 */

  byte mac[6];
  WiFi.macAddress(mac);
  my_hostname = HOSTNAME + String("-") + String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.hostname(my_hostname);
  #endif
  #ifdef ARDUINO_ARCH_ESP32
  {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    char __hostname[sizeof(my_hostname)+1];
    my_hostname.toCharArray(__hostname, sizeof(__hostname));
    WiFi.setHostname(__hostname);             // TODO not working
  }
  #endif
  #if DEBUG_ON>0
    debugMsgContinue(F("Using hostname: "));
    debugMsg(my_hostname);
  #endif
  WiFi.persistent(true);
  // Try to connect, if we have valid credentials
  boolean wlanConnected = false;
  #if DEBUG_ON>0
  debugMsg(F("Trying to connect to WLAN."));
  //debugMsg(esid); //not for multi
  #endif
  wlanConnected = connectToWLAN();    // try to connect as STA
  if (wlanConnected == false) {
    #if DEBUG_ON>0
    debugMsg(F("No connection, starting AP"));
    #endif    
    startAP(ap_ssid, ap_password);                                 // else, bring up AP
  }
    WiFi.persistent(false);

  IPAddress apIP = WiFi.softAPIP();
  IPAddress myIP = WiFi.localIP();
  #if DEBUG_ON>0
  debugMsgContinue(F("AP IP address: "));
  debugMsg(formatIPAsString(apIP));
  debugMsgContinue(F("WLAN IP address: "));
  debugMsg(formatIPAsString(myIP));
  #endif

  #if DEBUG_ON>0
    debugMsg(F("Starting Modbus"));
  #endif
  #ifdef ARDUINO_ARCH_ESP8266
    node.begin(mbAddr, Serial);
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    node.begin(mbAddr, mbSerial);
  #endif
  // Callbacks allow us to do half duplex without receiving our own transmissions
  // needed for Morningstar 6P6C connections, which are half-duplex
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  delay(10);

  model = getModel();
  if (model=="") { 
    noController = true;
    model = getModelFromEEPROM();
      if (model == "") {
    model = "PS-MPPT";
  }
  #if DEBUG_ON>0
    debugMsgContinue(F("Got model from EEPROM:"));
    debugMsg(model);
  #endif
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(F("Got model from mbus:"));
      debugMsg(model);
    #endif
    noController = false;
  }

  #if DEBUG_ON>0
    debugMsgContinue(F("Starting "));
    debugMsg(fs_type);
  #endif
  #ifdef ARDUINO_ARCH_ESP8266
    #ifdef FS_LITTLEFS
      LittleFSConfig cfg;
    #endif
    #ifdef FS_SPIFFS
      SPIFFSConfig cfg;
    #endif
    cfg.setAutoFormat(false);
    FILESYSTEM.setConfig(cfg);
  #endif
  if ( FILESYSTEM.begin() ) {
    #if DEBUG_ON>0
      debugMsgContinue(FS_TYPE);
      debugMsg(F(" opened"));
    #endif
    ;
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(FS_TYPE);
      debugMsg(F(" failed to open"));
    #endif
    ;    
  }

  #include "WebServer.h"              // start up web server

  mbTCP.begin();

  #ifdef ESP8266MDNS_LEGACY_H
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if (!MDNS.begin("MStar-WLAN", WiFi.localIP())) {
      #if DEBUG_ON>0
        debugMsg(F("Error setting up MDNS responder!"));
      #endif
      while (1) {
        delay(1000);
      }
    }
    MDNS.addService("http", "tcp", 80);
    #if DEBUG_ON>0
      debugMsg(F("mDNS responder started"));
    #endif
  #endif
  #if DEBUG_ON>0
    debugMsg("Getting modbus info for " + model);
  #endif
getFile(model);
getRTCTime();
// setAgingOffset(AGE_OFFSET);
  #if DEBUG_ON>0
    debugMsg(F("Leaving setup()"));
  #endif
} // setup()

void loop() {
  #ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtFeed();
    #ifdef ESP8266MDNS_LEGACY_H
      MDNS.update();
    #endif
  #endif

//  timerWrite(timer, 0);
  
  server.handleClient();
  #if DEBUG_ON==0
    serialPassthrough();
  #endif

// This handles connections to TCP/502 for Modbus TCP from MSView
  if (!mbClient.connected()) {
    mbClient = mbTCP.available();
  } else {
    mbusTCP();
  }

/*
 *  The rest just blinks the LED based on current connection state.
 */
  if (WiFi.softAPgetStationNum()) {            // AP client connected, flash slow
    blinkOnTime = 5;       
    blinkTopTime = 3000;
  } else if (WiFi.status() == WL_CONNECTED) {  // Connected as STA, flash 1/sec
    wlanConnected = true;
    blinkOnTime = 5;
    blinkTopTime = 1000;
  } else {                                     // no connections, almost nothing
    blinkOnTime = 2;
    blinkTopTime = 10000;
  }
  if (((millis() - lastMillis) > blinkTopTime) && blueLedState) {
    lastMillis = millis();
    blueLedState = false;
    setBlueLED(blueLedState);
  } else if (((millis() - lastMillis) > blinkOnTime) && !blueLedState) {
    blueLedState = true;
    setBlueLED(blueLedState);
  }
} // loop()
