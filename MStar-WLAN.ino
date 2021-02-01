 /*
 * MStar-WLAN 2020 mjs
 *   
 *  ESP32 - WROVER-B preferred for PSRAM. 16 MB flash is also preferred, But others are accmodated. 
 *  Included are custom partitions (e,g, WROVER 16 MB w/13 MB FS with modified files)
 *  
 *  Arduino IDE choices:
 *   lwIP: v2 higher bandwidth
 *   vTables: IRAM
 *   240 MHz
 *   
 *  ESP32 - WROOM (4 MB), use "data-small" folder for littlefs, put other files on SD-Card.
 *    -ESP32 Dev Module
 *    -Default 4 MB w/littlefs (1.2 MB APP/1.5MB littlefs)
 *  
 *   Original work, License CC BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 *   All rights reserved. Some parts subject to other licenses as noted.
 *   
 *   Libraries from Arduino IDE, unless otherwise noted.
 *   
 *   Using Arduino IDE 1.8.13, ESP32 Arduino 1.0.5rc1
 */

using namespace std; 
#define SOFTWARE_VERSION "v2.210131"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "ESP8266 support gone. Keep RTC in UTC. Dynamic updates of /status page.<br>\
                     Some changes for small flash. Change to ArduinoJSON 6, using PS_RAM.<br/>\
                     Allow WLAN and security settings. Allow reset to defaults. Change hostname.<br/>\
                     REST fixes. Get files from SD Card if not found on flash. Pulsing LED.<br/>\
                     Change to littlefs. Improve OTA. Serve important stuff from PROGMEM.<br/>\
                     Settable debug level. REST timeout. Modbus reliability. EEPROM>Preferences. <br/>\
                     Set controller logging period. Fahrenheit display. RRD/charting. Prep for email.<br/>\
                     Fix solar power calc. Fix min/max battV. Track current/max solar power correctly."

#define DEBUG_ON 1               // enable debugging output. If defined, debug_level can be changed during runtime.
                                 // 0 off, 1 least detail, 8 most detail, 9 includes passwords

#ifdef DEBUG_ON
  #define BAUD_LOGGER 115200        // baud rate for logging
  //#define DEBUG_ESP_HTTP_SERVER
  //#define DEBUG_ESP_CORE
  //#define EZT_DEBUG DEBUG           // for EZTime
#endif
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x) // see https://stackoverflow.com/questions/240353/convert-a-preprocessor-token-to-a-string
/*
 * If we define WIFI_MODE_AP_STA, it will both be an AP and try to connect as a station.
 * If not defined, we're a station if able to connect to a configured WLAN, otherwise
 * we turn into an AP. When an AP, if there are WLANs configured, it will try to connect
 * every few minutes if there is no client connected.
 */
#define WIFI_MODE_AP_STA                // define to run AP while also connected as station
#define PS_RAM    // use WROVER PS-RAM
#include <string>
#include <sstream>
#include <Preferences.h>
#include <FS.h>       // 1.0
#include <Wire.h>     // 1.0.1

#include "ESPAsyncWebServer.h"  // 1.2.0 
#include <WiFi.h>               // 1.0
#include <WiFiMulti.h>          //
#include <AsyncTCP.h>           // 1.0.3
#include <WebAuthentication.h> 
#include <ESPmDNS.h>
//#include <ESP32Ping.h>          // https://github.com/marian-craciunescu/ESP32Ping
#include <Update.h>
#include <FFat.h>                 // Needed for rrdtool
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#define USE_LITTLEFS
#ifdef USE_LITTLEFS
  #define FILESYSTEM LITTLEFS
  #define FS_NAME "littlefs"
  #include <LITTLEFS.h>         // GPL v2, https://spdx.org/licenses/BSD-3-Clause.html, LGPL https://github.com/marian-craciunescu/ESP32Ping/blob/master/LICENSE
#else
  #define FILESYSTEM SPIFFS
  #define FS_NAME "SPIFFS"
  #include <SPIFFS.h>
#endif
#include <SFFS.h>         // from Arduino IDE, BSD 3-Clause License https://github.com/pholmes2012/Simple_FRAM_FileSystem/blob/master/LICENSE 

#include <SD.h>                 // 1.0.5
#include <rrd.h>                // 1.0.1 - https://github.com/lbernstone/rrdtool_ESP32/ GPLv2
#include <SPI.h>
#include <sys/time.h>
//  #include "SdFat.h"
#include <HardwareSerial.h>
#include "driver/uart.h"
//#include "StreamUtils.h"              // 1.5.0 https://github.com/bblanchon/ArduinoStreamUtils (MIT License)
#include <ESP_Mail_Client.h>            // 1.0.13 https://github.com/mobizt/ESP-Mail-Client (MIT License)
#include <ArduinoJson.h>                // Benoit Blanchon 6.17.2, via IDE
#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
//#include <WiFiClientSecureAxTLS.h>
//#include <WiFiClientSecureBearSSL.h>

#include <WiFiServer.h>
//#include <WiFiServerSecure.h>
//#include <WiFiServerSecureAxTLS.h>
//#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>
#include <ModbusMaster.h> // 2.0.1 Doc Walker, via IDE
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "soc/ledc_reg.h"
#include "soc/rtc_wdt.h"



#define PROGMEM_FILES               // serves important files from PROGMEM instead of flash,
                                    // reduces dependence on filesystem. OTA.js, local.js, local.css
                                    // if files are found on SD Card, they will be used instead
File logFile;                         // platform log file
String logFileName;
File ctl_logFile;                     // controller log file
#define CTL_LOGFILE "/ctl00000000.csv"        // default, must start with /
String ctlLogFileName = CTL_LOGFILE;
#ifdef EZT_DEBUG
  File ezt_logFile;                   // ez-time log file
  #define EZT_LOGFILE "/eztime.log"   // must start with /
#endif


//---------------------------
// definitions
//---------------------------

#define PREF_REALM "MStar-WLAN"  // realm (namespace) for preferences
#define WROVER  // if using WROVER-B, not devkit
#define CELSIUS true   // display Celsius or Fahrenheit
// change with http://xxx/cmd?celsius=false  (or true)

//security
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "setup"
#define ROOT_USERNAME "admin"
#define ROOT_PASSWORD "update"
#define UPDATE_PATH "/update"
#define JSON_PASSWORD "imsure"
//#define nosec
#ifdef nosec
  #define WEB_USERNAME ""
  #define WEB_PASSWORD ""
  #define ROOT_USERNAME ""
  #define ROOT_PASSWORD ""
#endif

//wlan
#define AP_SSID "MStar"
#define AP_SSID_UNIQ                    // define to generate a unique SSID e.g. MStar-123456
// password, if used, must be 8-32 chars
#define AP_PSK  "morningstar"


#define HOSTNAME "MStarW"  // needs to be short - esp32 only does 12 char hostnames!

#define JSON_VERSION "1.0"                // changes with api changes
#define JSON_VERSION_MIN "1.0"            // changes when backward compatibility is broken
#define LEDC_timerbits          LEDC_TIMER_13_BIT

// GPIO 2 or 33 for WROVER-B board
#define WLAN_PIN_OLD  GPIO_NUM_2  // up through board 2020.2
#define WLAN_PIN      GPIO_NUM_33 // from 2020.10
#define RX_ENABLE_PIN GPIO_NUM_25 // RxEna to IO25, pin 10
#define RX_PIN        GPIO_NUM_27 // RxD to IO27, pin 12
#define TX_PIN        GPIO_NUM_4  // Txd to IO4, pin 26
#define SDA_PIN       GPIO_NUM_15 // GPIO 15  I2C SDA
#define SCL_PIN       GPIO_NUM_13 // GPIO 13  I2C SCL
#define SPI_SCLK      GPIO_NUM_18 // these are default pins for VSPI (SPI3)
#define SPI_MISO      GPIO_NUM_19 // just here for documentation
#define SPI_MOSI      GPIO_NUM_23
#define SD_CARD_CS    GPIO_NUM_5  // CS for SDCard
#define FRAM_CS       GPIO_NUM_22
#define SD_DETECT     GPIO_NUM_26 // SD card inserted, only for SDCard0
#define BOOT_SW       GPIO_NUM_0  // GPIO0 connected to BOOT switch
#define I2C_SDA_RESET GPIO_NUM_21 // https://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html
#ifdef WROVER
  #define USB_CC1_PIN GPIO_NUM_34  // USB Type-C CC1
  #define USB_CC2_PIN GPIO_NUM_35
  #define USB_PWR_DET GPIO_NUM_32  // to VCCIO
#endif
#define SD_CARD_LOG true           //log to SD card
#define CTLRRDFILENAME  /ctl00000000.rrd    // must start with /
#define CTLDRRDFILENAME /ctl00000000d.rrd    // must start with /
bool sd_card_log = SD_CARD_LOG;
bool sd_card_available = false;
volatile bool sd_card_changed = false;
bool needLogTime = true;  // whether we need to print time to the log

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
/*
string web_username = WEB_USERNAME;
string web_password = WEB_PASSWORD;
string json_password = JSON_PASSWORD;
string json_version = JSON_VERSION;
string json_version_min = JSON_VERSION_MIN;
string ap_ssid = AP_SSID;
string ap_password = AP_PSK;
string ap_SSID = AP_SSID;
string root_username = ROOT_USERNAME;
string root_password = ROOT_PASSWORD;
string serialNumber = SERIAL_NUMBER;
*/
String web_username = WEB_USERNAME;
String web_password = WEB_PASSWORD;
String json_password = JSON_PASSWORD;
String json_version = JSON_VERSION;
String json_version_min = JSON_VERSION_MIN;
String ap_ssid = AP_SSID;
String ap_password = AP_PSK;
String ap_SSID = AP_SSID;
String root_username = ROOT_USERNAME;
String root_password = ROOT_PASSWORD;
String serialNumber = SERIAL_NUMBER;

const char *update_path = UPDATE_PATH;

String esid[4];
String epass[4];
int wlan_count=0;             // holds how many WLANs configured in EEPROM
boolean wlanRead = false;     // if we've read SSID/PSKs from EEPROM
boolean wlanSet = false;      // if we've added them to wifiMulti
boolean wlansAdded = false;   // if we've added WLANs to wifiMulti
boolean largeFlash = false;
enum ledStatusStates {lss_APCLIENT, lss_STATION, lss_IDLE, lss_NONE};
ledStatusStates blinkyState = lss_NONE;
boolean led_change_done = true;
boolean noController = true;
boolean psRAMavail = false;
boolean FRAMavail = false;
boolean daytime = true;
boolean celsius = CELSIUS;  // temperature in Celsius?
boolean vfs_fat_registered = false;
uint8_t myWDT=120;
time_t bootTime, nightTime;
unsigned long lastWLANtry;      // when we last tried to connected (or tried) to an AP
int mbAddr = mbusSlave;
String model = MODEL;
String fullModel = MODEL;
String ctlSerialNum = "00000000";
string sd_vfs_path = "/sdvfs";
string ctlRrdFileName = TOSTRING(CTLRRDFILENAME);
string ctlRrdFileFullPath = sd_vfs_path + ctlRrdFileName;
string ctlDRrdFileName = TOSTRING(CTLDRRDFILENAME);
string ctlDRrdFileFullPath = sd_vfs_path + ctlDRrdFileName;

unsigned long mbustries, mbuserrs, mbuserrs_recovered, lastFound = millis();
float vary;
byte mac[6];
String my_MAC;
String my_name;                 // for log file name
String my_hostname;             // for networking
String logLast="";
#define LOG_FREQ  15            // minutes between log entries
unsigned int log_freq = LOG_FREQ; 
static char log_print_buff[512];
FATFS* FATfs = NULL;
File fsUploadFile;              // a File object to temporarily store the received file

String referrer; 
ModbusMaster node;
WiFiServer modbusTCP(502);
WiFiClient modbusClient;
AsyncWebServer server(80);
AsyncCallbackWebHandler rrdHandler;
AsyncCallbackWebHandler drrdHandler;
WiFiMulti wifiMulti;
//  WebServer server(80);
HardwareSerial mbSerial(1);
ledc_channel_config_t ledc_channel[2];  //
Preferences preferences;

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

#include "WebCmd.h"
#include "Setups.h"
#include "OTA.h"
#include "Web.h"                // starts up web server


void setup() {
  pinMode(I2C_SDA_RESET, INPUT);
  pinMode(BOOT_SW, INPUT_PULLUP); // to read the boot switch
  #ifdef WROVER
    pinMode(USB_PWR_DET, INPUT);
    adcAttachPin(USB_CC1_PIN);
    adcAttachPin(USB_CC2_PIN);
    analogSetPinAttenuation(USB_CC1_PIN, ADC_6db);
    analogSetPinAttenuation(USB_CC2_PIN, ADC_6db);
  #endif
  attachSDCardIRQ();            // get an interrupt when card inserted/removed
    
  WiFi.macAddress(mac);
  my_MAC = "0" + String(mac[5],HEX); // this pads the octets
  my_MAC = "0" + String(mac[4],HEX) + my_MAC.substring(my_MAC.length()-2) ;
  my_MAC = "0" + String(mac[3],HEX) + my_MAC.substring(my_MAC.length()-4) ;
  my_MAC = my_MAC.substring(my_MAC.length()-6) ;
  my_name = HOSTNAME + String("-") + my_MAC;
  my_hostname = my_name;
  logFileName = "/"+ my_name + ".log";
  setupRtcSQW();  // real time clock 1/sec interrupt
  setupComms();   // serial port stuff
  setupFS();      // filesystems

  if ( ESP.getSketchSize() + 4096 < ESP.getFreeSketchSpace() ) {
    largeFlash = true;
  }

  if (psramInit()){
    psRAMavail=true;
    debugMsg(F("PS-RAM available"),1);
    #ifdef PS_RAM
      debugMsgln(F("(using)"),1);
    #endif
    #ifndef PS_RAM
      debugMsgln(F("(not using)"),1);
      psRAMavail=false;
    #endif    
  }

 
  preferences.begin("eeprom", false);   // this bit can go away in the future
  preferences.clear();                  // just here to clear old eeprom from nvs
  preferences.end();

  getPreferences();
  getWLANs();
  setupWLAN();
  setupClocks();
  setupModbus();
  modbusTCP.begin();
  debugMsgln("Getting modbus info for " + model,1);
  getFile(model);
  if (getSn()) {
    debugMsg(F("Controller serial number: "),1);
    debugMsgln(ctlSerialNum,1);
  } else {
    debugMsgln(F("Failed to get serial number."),1);
  }
  
  refreshCtlLogFile();

  server.begin();
  startWeb();
  getRtcTime();
  WiFi.setTxPower(WIFI_POWER_19dBm); // 15 is about 1/3 the power of the default 19.5 dBm

  setEvents(); // setup regular events.
  blinkySetup(WLAN_PIN,WLAN_PIN_OLD);
  blinky(1,ULONG_MAX,0,0); // LED OFF
  debugMsgln(F("Leaving setup()"),1);
} // setup()

void loop() {
  events(); // for EZTime  
  checkNtp();
  if (sd_card_changed) changeSDCard();
  #ifndef DEBUG_ON
    serialPassthrough();
  #endif
  asynchOTA.loop();
// This handles connections to TCP/502 for Modbus TCP from MSView
  if (!modbusClient.connected()) {
    modbusClient = modbusTCP.available();
  } else {
    mbusTCP();
  }

  long unsigned int boot_reset = millis() | 1; // can never be 0 (arithmatic or)
  while (!digitalRead(BOOT_SW)) {             // BOOT switch is pressed
    delay(100);
    if (millis() > boot_reset + 5000) {       // switch pressed for more than 5 seconds
      boolean wlanLedState = true;
      boot_reset = 0;                         // doing a reset
      wlanLedState = !wlanLedState;
      setWlanLED(wlanLedState);
      delay(50);                             // blink fast if switch pressed and doing reset
    }
  }                                           // keep looping as long as BOOT is pressed
  if (boot_reset == 0) {                      // reset to defaults requested
    debugMsgln(F("BOOT held, resetting to defaults."),1);    
    resetPreferences();
    reboot();
  }

/*
 *  Blink the LED based on current connection state.
 */
  if (led_change_done) blinky();                        // maintenance for LED, no need to call if still changing
  if (WiFi.softAPgetStationNum()) {                     // AP client connected, fast
    if ( blinkyState != lss_APCLIENT ) {
      blinkyState = lss_APCLIENT;
      blinky(300,1000);
    }
  } else if (WiFi.status() == WL_CONNECTED) {           // Connected as STA, flash every 2.5 sec
    if ( blinkyState != lss_STATION ) {
      blinkyState = lss_STATION;
      blinky(750, 2500, 255, 8);
    }
  } else {                                              // no connections, slower/dimmer
    if ( blinkyState != lss_IDLE ) {
      blinkyState = lss_IDLE;
      blinky(5000, 5000, 64, 4);
    }
  }
  myWDT = 60; // reset watchdog

} // loop()
