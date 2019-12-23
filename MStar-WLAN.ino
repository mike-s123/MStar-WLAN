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

#define SOFTWARE_VERSION "v1.191223"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "Refactor for different controller families. ESP32 working.<br>\
                     wifiMulti (no GUI). WLAN robustness. WIFI_AP_STA support.<br>\
                     More WLAN work. Started adding time support."

#define DEBUG_ON 1                // enable debugging output. 0 currently causes issues (12/22/2019)
                                  // 0 off, 1 least detail, 5 most detail, 6 includes passwords
                                  // 0 not working on ESP32 for now
//#define debugjs                   // ifdef, overrides servestatic to avoid caching of local.js
//#define debugcss                  // ditto, for css
#ifdef DEBUG_ON
  #define BAUD_LOGGER 115200        // for software serial logging out "old" pins
                                    // because we're swapping the UART to new ones
  #define DEBUG_ESP_PORT logger
  //#define DEBUG_ESP_HTTP_SERVER
  //#define DEBUG_ESP_CORE
#endif
#include <string>
#include <sstream>
#include <EEPROM.h>
#include <FS.h>

#include <Wire.h>
#include <DS3231.h>   // Andrew Wickert, et al 1.0.2, via IDE
#include <extEEPROM.h> // Jack Christensen, 3.4.1, via IDE licensed under CC BY-SA 4.0.
//#include <BearSSLHelpers.h>
//#include <CertStoreBearSSL.h>
#ifdef ARDUINO_ARCH_ESP8266
  #include <SoftwareSerial.h>
  ADC_MODE(ADC_VCC);
  #if DEBUG_ON>0
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
//#include <WiFiServerSecfure.h>
//#include <WiFiServerSecureAxTLS.h>
//#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>

#include <ModbusMaster.h> //Doc Walker 2.0.1, via IDE
#include <ezTime.h> //Rop Gonggrijp 1.8.3, via IDE

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

#define HOSTNAME "MStarWLAN"

#define JSON_VERSION "1.0"                // changes with api changes
#define JSON_VERSION_MIN "1.0"            // changes when backward compatibility is broken

// GPIO16 (D0) on NodeMCU, 1 on ESP-1, but won't work on ESP-1 because of serial?
// GPIO 2 for blue led on ESP-12E, GPIO 16 (or BUILTIN_LED) for blue led on NodeMCU
// GPIO 2 (D4) for Wemos D1 mini
#define WLAN_PIN 2
//#define WLAN_PIN BUILTIN_LED

#define EEPROM_SIZE 512
#define EEPROM_SIG "mjs!"
/*
 * "EEPROM" on ESP
 * 0-127   4x32 WLAN SSID
 * 128-255 4x32 WLAN password
 * 256-272 16 Controller model
 * 272-303 (32) ntp server
 * 304-305 (2)  ntp poll interval (u_int 16)
 * 306-337 (32) ntp POSIX timestring
 * 508-511 (4)  Valid signature (EEPROM_SIG)
 */
#define eeWLANSSID 0
#define eeWLANPASS 128
#define eeModel 256
#define eeNtpServer 272
#define eeNtpPoll 304
#define eeNtpTZ 306
 
#define CLK_EEPROM_SIZE 4096
#define CLK_EEPROM_SIG "mjs!"
/*
 * EEPROM on DS3231 module
 * 0    (1) Last set aging offset (so we have it if battery goes away)
 * 1-2  (2) Year
 * 3    (1) Month
 * 4    (1) Day
 * 5    (1) Hour
 * 6    (1) Minute
 * 7    (1) Second
 * 8-11 (4) Last time set (Unix time)
 * 
 * 4092-4095 (4)  Valid signature (CLK_EEPROM_SIG)
 */
#define eeRtcAge 0
#define eeRtcYear 1
#define eeRtcMonth 3
#define eeRtcDay 4
#define eeRtcHour 5
#define eeRtcMinute 6
#define eeRtcSecond 7
#define eeRtcLastSetTime 8

#define DEFAULT_NTP_TZ "EST5EDT,M3.2.0,M11.1.0"   //POSIX format
#define DEFAULT_NTP_INTERVAL 7207                 // seconds, best if not a multiple of 60
#define DEFAULT_NTP_SERVER "0.pool.ntp.org"
#define MIN_NTP_INTERVAL 601                      // seconds
#define MAX_NTP_INTERVAL 64999                    // ~18 hours

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
int blinkTopTime = 2000;
unsigned long lastMillis = 0;
unsigned long lastWLANtry;      // when we last tried to connected (or tried) to an AP
int mbAddr = mbusSlave;
String model = MODEL;
String fullModel = MODEL;
byte mac[6];
String my_hostname;
File fsUploadFile;              // a File object to temporarily store the received file

String referrer; 
IPAddress apIP(192, 168, 99, 1);
ModbusMaster node;
DS3231 Clock;
extEEPROM clk_eeprom(kbits_32, 1, 32, 0x57);
#define DS3231_I2C 0x68
#define AT24Cxx_BASE_ADDR 0x57 // ZS-042/HW-84 modules have pullups on A0-A2
#define AGE_OFFSET 0       // aging offset for ds3231, higher is slower
                            // ~0.1 ppm per (~9 ms/day, 0.26 sec/month), higher is slower, 
                            // 11.6 ppm is ~ 1 sec/day
bool useRTC = false;
bool twentyFourHourClock = true;
bool Century=false;
bool h12;
bool PM;
String ntpServer = DEFAULT_NTP_SERVER;
unsigned short int ntpInterval = MAX_NTP_INTERVAL;
String ntpTZ = DEFAULT_NTP_TZ;  //posix string
String tzName;  // used for lookup
String tzPosix; // used for lookup
Timezone myTZ;


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
  unsigned short int ntp_temp;
  #ifdef ARDUINO_ARCH_ESP8266
    digitalWrite(SELF_RST_PIN, HIGH); // So board can do a hardware reset of itself
    pinMode(SELF_RST_PIN, OUTPUT);
  #endif
  pinMode(SQW_PIN, INPUT);            // From DS3231 square wave output 1 Hz - 8.192 KHz
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
//     cSerial = new SoftwareSerial(3, 1);
//      setupPassthru();
//      delay(100);
//      cSerial->println(F("cSerial setup"));
    #endif
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    /* remap UART1 
     *  
     *  pinMatrixOutAttach(uint8_t pin, uint8_t function, bool invertOut, bool invertEnable);
     *  pinMatrixInAttach(uint8_t pin, uint8_t signal, bool inverted);
    */
    #if DEBUG_ON>0
      setupDebug();
      debugMsgContinue(F("Debug on, level "));
      debugMsg(String(DEBUG_ON));
    #endif
    pinMode(RX_PIN, INPUT);
    delay(100);
    mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);    
//    pinMatrixOutAttach(TX_PIN, U1TXD_OUT_IDX, true, false);
//    pinMatrixInAttach(RX_PIN, U1RXD_IN_IDX, true);
  #endif

    Wire.begin(SDA_PIN, SCL_PIN);              // setup I2C
    if (!Wire.requestFrom(DS3231_I2C, 2)) {
      #if DEBUG_ON>0
        debugMsgContinue(F("No "));
      #endif      
    } else {
     useRTC = true;
    }
    #if DEBUG_ON>0
      debugMsg(F("RTC found"));    
      byte i2cStat = clk_eeprom.begin(clk_eeprom.twiClock100kHz);
      if ( i2cStat != 0 ) {
        debugMsg(F("I2C Problem with RTC eeprom"));
      }
    #endif
      
     if (useRTC) {
      delay(10);
      if (!Clock.oscillatorCheck()) {  // check for Oscillator Stopped Flag (!good RTC)
        #if DEBUG_ON>0
          debugMsg(F("Initializing RTC"));
        #endif
        Clock.enableOscillator(true, true, 0);  // Ena osc, sqw on batt, 1 Hz
        delay(100);
        setRTC(true);   // Clears OSF flag?
        delay(2);
        if (!Clock.oscillatorCheck()) {
          useRTC = false;
          #if DEBUG_ON>0
            debugMsg(F("RTC check failed"));  // probably no RTC
          #endif
        } 
      }
    }  

  pinMode(RX_ENABLE_PIN, OUTPUT);            // used for half-duplex MODBUS
  rxEnable(false);
  pinMode(WLAN_PIN, OUTPUT);  // LED

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

/*
 * WiFi setup
 */

  WiFi.macAddress(mac);
  #ifdef AP_SSID_UNIQ
    ap_SSID.append("-");
    ap_SSID.append((String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX)).c_str());
  #endif
  ap_ssid = ap_SSID.c_str();
  //ap_password = AP_PSK;
  my_hostname = HOSTNAME + String("-") + String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);

WiFi.persistent(true);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.hostname(my_hostname);
  #endif
  
  #ifdef ARDUINO_ARCH_ESP32
  {
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

  #if DEBUG_ON>0
    debugMsg(F("Trying to connect to WLAN."));
  #endif
  
  wlanConnected = connectToWLAN();    // try to connect as STA
  lastWLANtry = millis();


  if (WiFi.isConnected()) { //connected as a station
    myTZ.setDefault();                     // setup NTP service
    tzPosix = getNtpTZFromEEPROM();
  //  if (tzPosix != "") {
  //   tzPosix = ntpTZ);
  //  }
    #if DEBUG_ON>0
      debugMsgContinue(F("Setting NtpTZ to:"));
      debugMsg(tzPosix);
    #endif
    ntpTZ = tzPosix;
    myTZ.setPosix(tzPosix);
    tzPosix = getNtpServerFromEEPROM(); // tzPosix is used as a temp
     #if DEBUG_ON>0
      debugMsgContinue(F("Setting NtpServer to:"));
      debugMsg(tzPosix);
    #endif
    setServer(tzPosix);
    ntpServer = tzPosix;
    tzPosix = ""; // we were just using it temporarily
    ntp_temp = getNtpPollFromEEPROM();
     #if DEBUG_ON>0
      debugMsgContinue(F("Setting NtpPoll to:"));
      debugMsg(String(ntp_temp));
    #endif
    setInterval(ntp_temp);
    ntpInterval = ntp_temp;
    
    if (timeStatus() == timeNotSet) {      // try to sync with NTP a few times
      #if DEBUG_ON>0
        debugMsg(F("Trying to sync NTP."));
      #endif
      for (int idx = 0 ; idx < 3 ; idx++) {
        if (waitForSync(3)) {
          idx = 4;
        }
      }
    }
    #if DEBUG_ON>0
      if (timeStatus() == timeSet) {
        debugMsgContinue(F("Sync'd to NTP. Time is "));
        debugMsg(myTZ.dateTime(RFC850));
      } else if (useRTC) {
        debugMsgContinue(F("Didn't sync NTP. Using RTC time."));
        setInterval(0);  // disable ntp
        setTime(getUnixTime()); // from RTC
      }
    #endif
  }
  #ifndef WIFI_MODE_AP_STA
    if (!wlanConnected) {             // If can't connect in STA mode, switch to AP mode
      startAP(ap_ssid, ap_password);
    }
  #endif
  
  IPAddress apIP = WiFi.softAPIP();
  IPAddress myIP = WiFi.localIP();
  
  #if DEBUG_ON>0
  debugMsgContinue(F("AP IP address: "));
  debugMsg(formatIPAsString(apIP));
  if (wlanConnected) {
    debugMsgContinue(F("WLAN IP address: "));
    debugMsg(formatIPAsString(myIP));
    debugMsg("Connected to:" + String(WiFi.SSID()));
  }
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
 *  Blink the LED based on current connection state.
 */
 
  if (WiFi.softAPgetStationNum()) {                     // AP client connected, every 3 sec
    blinkOnTime = 5;       
    blinkTopTime = 3000;
  } else if (WiFi.status() == WL_CONNECTED) {           // Connected as STA (or AP), flash 1/sec
    wlanConnected = true;
    blinkOnTime = 5;
    blinkTopTime = 1000;
  } else {                                              // no connections, every 10 sec
    blinkOnTime = 2;
    blinkTopTime = 10000;  
  }

  /* 
   *  If we have configured WLANs and are not connected as a station, try to connect every few minutes
   *  Only if running as both AP and STA (doesn't take AP down, but will be non-responsive to client
   *  while scanning). If running as STA -or- AP, don't scan if client is connected.
   */
  #ifdef WIFI_AP_STA  // if AP_STA mode, ok to try with client connected
    if (( wlan_count && !WiFi.isConnected() &&(millis() - lastWLANtry) > 300000) ) {  // 5 minutes
  #else               // if not, would disconnect client, so check
    if (( wlan_count && !WiFi.isConnected() && !WiFi.softAPgetStationNum() &&(millis() - lastWLANtry) > 30000) ) {
  #endif
    
    #if DEBUG_ON>1 
      debugMsg("lastWLANtry="+String(lastWLANtry));
      debugMsg(F("Trying WLAN connection"));
    #endif
    tryWLAN();
    lastWLANtry = millis();   // mark attempt
  }

  if (((millis() - lastMillis) > blinkTopTime) && wlanLedState) {
    lastMillis = millis();
    wlanLedState = false;
    setWlanLED(wlanLedState);
  } else if (((millis() - lastMillis) > blinkOnTime) && !wlanLedState) {
    wlanLedState = true;
    setWlanLED(wlanLedState);
  }
} // loop()
