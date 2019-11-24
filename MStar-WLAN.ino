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
 *   ESP32 - M5Stack-FIRE, Large SPIFFS (7 MB)
 *   cpu: fastest
 *   flash: 2M+ SPIFFS
 *   lwIP: v2 higher bandwidth
 *   vTables: IRAM
 *   
 *   Original work, License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 *   some parts subject to other licenses as noted.
 */

#define SOFTWARE_VERSION "v0.63"
#define SERIAL_NUMBER "000001"
#define BUILD_NOTES "Updated SDK, readDeviceID working to select model,</br>\
DS3231 clock, new board layout, array watts, ESP8266 ping."

#define DEBUG_ON 3                // enable debugging output
                                  // 0 off, 1 least detail, 5 most detail, 6 includes passwords
                                  // 0 not working on ESP32 for now
#define BAUD_LOGGER 115200        // for software serial logging out "old" pins
                                  // because we're swapping the UART to new ones

#include <string>
#include <sstream>
#include <EEPROM.h>
#include <FS.h>
#include <Wire.h>
#include <DS3231.h>   // Andrew Wickert, et al 1.0.2
#include <ZEeprom.h>  // Pierre Valleau 1.0.0
//#include <BearSSLHelpers.h>
//#include <CertStoreBearSSL.h>
#ifdef ARDUINO_ARCH_ESP8266
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
  #include <ESP8266mDNS.h>
  /* This will compile in OTA support - need at least 1 MB for OTA updates.
     we check available space at runtime before allowing it.
  */
  #include <ESP8266HTTPUpdateServer.h>
  #include <SoftwareSerial.h>
//  #include <ESP8266Ping.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <WebServer.h>
//#include "ESPAsyncWebServer.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#endif

#include <ArduinoJson.h>   // Benoit Blanchon 5.13.4
#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
//#include <WiFiClientSecureAxTLS.h>
//#include <WiFiClientSecureBearSSL.h>

#include <WiFiServer.h>
//#include <WiFiServerSecure.h>
//#include <WiFiServerSecureAxTLS.h>
//#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>

#include <ModbusMaster.h> //Doc Walker 2.0.1

//---------------------------
// definitions
//---------------------------

//security
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "setup"
#define UPDATE_USERNAME "admin"
#define UPDATE_PASSWORD "update"
#define UPDATE_PATH "/update"
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

#define JSON_VERSION "1.0"               // changes with api changes
#define JSON_VERSION_MIN "1.0"           // changes when backward compatibility is broken

// GPIO16 (D0) on NodeMCU, 1 on ESP-1, but won't work on ESP-1 because of serial?
// GPIO 2 for blue led on ESP-12E, GPIO 16 (or BUILTIN_LED) for blue led on NodeMCU
// GPIO 2 (D4) for Wemos D1 mini
#define WIFI_PIN 2
//#define WIFI_PIN BUILTIN_LED

#define FILESYSTEM SPIFFS

#define EEPROM_SIZE 512
#define EEPROM_SIG "mjs!"
/*
 * 
 * 0-31    (32) WLAN SSID
 * 32-95   (64) WLAN password
 * 96-111  (16) Controller model
 * 
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
  #define RX_ENABLE_PIN 23  // RxEna to IO23, pin 37
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
bool useRTC = true;
bool twentyFourHourClock = true;
bool Century=false;
bool h12;
bool PM;

#ifdef ARDUINO_ARCH_ESP8266
  ESP8266WebServer server(80);
  ESP8266HTTPUpdateServer httpUpdater;
#endif
WiFiServer mbTCP(502);
WiFiClient mbClient;
  
#ifdef ARDUINO_ARCH_ESP32
//  AsyncWebServer server(80);
  WebServer server(80);
  HardwareSerial mbSerial(1);
#endif

#ifdef ARDUINO_ARCH_ESP8266
  ADC_MODE(ADC_VCC);
  #if DEBUG_ON>0
    SoftwareSerial* logger = nullptr;
  #else
    SoftwareSerial* cSerial = nullptr;  
  #endif
#endif

int Year = 2019;
byte Month = 4, Day = 1, Weekday = 1, Hour = 0, Minute = 0, Second = 0;  // April fool's

//order here is important
#include "Utility.h"
#include "edit.h"
#include "MBus.h"
#include "ControllerFiles.h"
#include "HTML.h"
#include "RestPage.h"

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
    Serial.swap();  //use alternate UART pins, so now GPIO 15(D8,TX)/13(D7,RX)
    #if DEBUG_ON>0
      logger = new SoftwareSerial(3, 1);
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
  String esid = getSSIDFromEEPROM();
  String epass = getPasswordFromEEPROM();
  #if DEBUG_ON>0
    debugMsgContinue(F("esid:"));
    debugMsg(esid);
  #endif
  #if DEBUG_ON>5
    debugMsg("epass= "+epass);  
  #endif

/*
 * WiFi setup
 */

  WiFi.persistent(true);
  // Try to connect, if we have valid credentials
  boolean wlanConnected = false;
  if (esid.length() > 0) {
    #if DEBUG_ON>0
    debugMsgContinue(F("Trying to connect to SSID "));
    debugMsg(esid);
    #endif
    wlanConnected = connectToWLAN(esid.c_str(), epass.c_str());    // try to connect as STA
  } else {
    wlanConnected = false;
  }
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
    debugMsg(F("Starting SPIFFS"));
  #endif
  SPIFFS.begin();
  
  server.on("/",               statusPageHandler);
  server.on(F("/status"),      statusPageHandler);
  
  server.on(F("/platform"), platformPageHandler);

  server.on(F("/setTime"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setTimePageHandler();
  });

  server.on(F("/cmd"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return cmdPageHandler();
  });

  server.on(F("/setcharge"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setchargePageHandler();
  });

  server.on(F("/setother"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setotherPageHandler();
  });

  server.on(F("/rest"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return restPageHandler();
  });

  server.on(F("/allregs"),      allregsPageHandler);
  server.on(F("/allcoils"),     allcoilsPageHandler);

  server.on(F("/wlan_config"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return wlanPageHandler();
  });
  server.on(F("/utility"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return utilityPageHandler();
  });

  server.on(F("/getfile"),   getfilePageHandler);

  server.on(F("/reset"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return resetPageHandler();
  });

  server.on(F("/resetall"), []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return resetAllPageHandler();
  });

  server.on("/list", HTTP_GET, handleFileList);
  //load editor

  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  server.onNotFound([]() {                              // If the client requests any URI
    #if DEBUG_ON>2
      debugMsg("server.onNotFound");
    #endif
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, F("text/plain"), F("404: Not Found")); // otherwise, respond with a 404 (Not Found) error
  });
  
  //TODO doesn't work on ESP32
  server.serveStatic("/", SPIFFS, "/", "max-age=43200"); // tell browser to cache SPIFFS stuff for 12 hours

  #ifdef ARDUINO_ARCH_ESP32
    #if DEBUG_ON>2
      debugMsg("ESP32 server.ons");
    #endif
  /*handling uploading firmware file */
  // TODO - return client to home page after success
    server.on("/update", HTTP_GET, []() {
    #if DEBUG_ON>2
      debugMsg("ESP32 server.on /update");
    #endif
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    if (!handleFileRead("/updatePage.htm"))                  // send it if it exists
    server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.on("/updater", HTTP_POST, []() {
//    if (!server.authenticate(web_username, web_password)) {
//      return server.requestAuthentication();
//    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(50);
    reboot();
  }, []() {
    HTTPUpload& upload = server.upload();
    #if DEBUG_ON>0
      if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          debugMsg("Update Success: " + String(upload.totalSize));
        } else {
          Update.printError(Serial);
        }
      }
    #endif
  });
  #endif // ARDUINO_ARCH_ESP32
  
  server.begin();
  #if DEBUG_ON>0
  debugMsg(F("HTTP server started"));
  #endif

  mbTCP.begin();
  
  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp8266.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin("MStar-WLAN")) {
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
    MDNS.update();
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
    #ifdef ESP8266Ping_H
      if ((millis() - lastMillis) > blinkTopTime * 5) {  // every 5 seconds
 //       IPAddress ping_ip (172, 31, 255, 254); // The remote ip to ping
        IPAddress ping_ip = WiFi.gatewayIP();
        Ping.ping(ping_ip);  // just to keep wifi going, don't care if it works.
      }
    #endif
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



//-------------------------------------------------------------------
//------------------------- Page handlers  --------------------------
//-------------------------------------------------------------------

void setotherPageHandler() {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  #if DEBUG_ON>0
    debugMsg(F("Entering /setother page."));
  #endif
   
  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"controller\"><h3>");
  if (noController) {
    response_message += F("No Controller");
  } else {
    response_message += model;
    if (controllerNeedsReset()) response_message += F(" (Controller needs restart)");
  }
  response_message += F("</h3></div>"); 

//  response_message += F("<center><img src=\"Other.png\"></center>");
  String heading = F("Other Settings");
  heading += "<h2 id=\"warning\"></h2>";

  response_message += getTableHead2Col(heading, F("Description"), F("Value")); 

  String registers[] = {F("EV_lvd"), F("EV_lvr"), F("EV_hvd"), F("EV_hvr"), F("EV_lhvd"), F("EV_lhvr"), \
                        F("ER_icomp"), F("Et_lvd_warn"), \
                        F("EV_soc_g_gy"), F("EV_soc_gy_y"), F("EV_soc_y_yr"), F("EV_soc_yr_r") };

  for (int i = 0 ; i < 12 ; i++ ) { 
    addr = findAddrByVar(registers[i]);
    if (addr >= 0) {
      fullReg reg;
      String value;
      mbGetFullReg(reg, addr);
      String inputvar = "<input type=\"number\" id=\"";
      inputvar += reg.var;
      inputvar += "\" size=\"9\" value=\"";
      inputvar += reg.value;
      inputvar += "\" onchange=\"writeReg('";
      inputvar += addr;
      inputvar += "', this.value, '";
      inputvar += reg.var;
      inputvar += "')\">&nbsp";
      response_message += getTableRow2Col(reg.var+" ("+reg.desc+")", inputvar + mbRegUnitName[reg.unit] );
    }
  }

  response_message += getTableFoot();
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

void setchargePageHandler() {
/*  
 *   Page to set controller charging settings.
 */
  int addr, result;
  String desc, val;
  #if DEBUG_ON>0
    debugMsg(F("Entering /setcharge page."));
  #endif

  checkController();

  String response_message;
  response_message.reserve(5000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"controller\"><h3>");
  if (noController) {
    response_message += F("No Controller");
  } else {
    response_message += model;
    if (controllerNeedsReset()) response_message += F(" (Controller needs restart)");
  }
  response_message += F("</h3></div>"); 

  
  response_message += F("<center><img src=\"Charging.png\"></center>");  
  String heading = F("Charge Settings");
  heading += "<h2 id=\"warning\"></h2>";

  response_message += getTableHead2Col(heading, F("Description"), F("Value")); 

  // TODO - make sure this is standardized
  String registers[] = {F("EV_reg"), F("Et_float"), F("EV_float"), F("EV_floatlb_trip"), F("Et_floatlb"), F("EV_eq"), \
                      F("Et_eq_reg"), F("Et_eq_above"), F("EV_float_cancel"),F("Et_float_exit_cum"), \
                      F("Evb_ref_lim"), F("Evb_ref_charge_lim"), F("Et_eqcalendar"), \
                      F("EV_tempcomp"), F("ETb_max"), F("ETb_min") };

  for (int i = 0 ; i < 16 ; i++ ) { 
    addr = findAddrByVar(registers[i]);
    if (addr >= 0) {
      fullReg reg;
      String value;
      mbGetFullReg(reg, addr);
      String inputvar = "<input type=\"number\" id=\"";
      inputvar += reg.var;
      inputvar += "\" size=\"9\" value=\"";
      inputvar += reg.value;
      inputvar += "\" onchange=\"writeReg('";
      inputvar += addr;
      inputvar += "', this.value, '";
      inputvar += reg.var;
      inputvar += "')\">&nbsp";
      response_message += getTableRow2Col(reg.var+" ("+reg.desc+")", inputvar + mbRegUnitName[reg.unit] );
    }
  }

  response_message += getTableFoot();
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

void cmdPageHandler() {                          
/*
 *  Handles POST requests made to /cmd
 *  Used by JavaScript?
 */
  int addr, offset, numArgs = server.args();
  String data, value, response_message = F("OK"), rtcTime;
  enum commands { read_reg, write_reg, read_coil, write_coil, set_rtc, set_aging };
  commands cmd;
  #if DEBUG_ON>2
    debugMsgContinue(F("SET args received:"));
    debugMsg(String(numArgs));
  #endif  
  for (int i=0 ; i<numArgs ; i++) {
    #if DEBUG_ON>2
      debugMsg("SET arg#"+String(i)+", "+server.argName(i)+":"+server.arg(i));
    #endif
    if ( server.argName(i) == F("writereg") ) {
      cmd = write_reg;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("readreg") ) {
      cmd = read_reg;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("writecoil") ) {
      cmd = write_coil;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("readcoil") ) {
      cmd = read_coil;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("setrtc") ) {
      cmd = set_rtc;
      rtcTime = server.arg(i);
    } else if ( server.argName(i) == F("setagingoffset") ) {
      cmd = set_aging;
      offset = server.arg(i).toInt();
    } else if ( server.argName(i) == F("data") ) {
      data = server.arg(i);
    }
  }
  switch (cmd) {
    case read_reg:  MBus_get_reg(addr, value);
                    response_message = value;
                    break;
    case write_reg: MBus_write_reg(addr, data);
                    break;
    case read_coil: bool state;
                    MBus_get_coil(addr, state);
                    (state)?value=F("on"):value=F("off");
                    response_message = value;
                    break;
    case write_coil: MBus_write_coil(addr, data);
                    break;
    case set_rtc:   setRtcTime(rtcTime);
                    break;
    case set_aging: setAgingOffset(offset);
                    break;
    default:        response_message = F("err");
  }
  server.send(200, F("text/plain"), response_message);
}

void platformPageHandler()
/*
 * Returns a page with info on the ESP platform.
 */
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /platform page."));
  #endif

  checkController();

  String response_message;
  response_message.reserve(5000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<div class=\"controller\"><h3>");
  if (noController) {
    response_message += F("No Controller");
  } else {
    response_message += model;
    if (controllerNeedsReset()) response_message += F(" (Controller needs restart)");
  }
  response_message += F("</h3></div>"); 


  // Status table
  response_message += getTableHead2Col(F("Current Status"), F("Name"), F("Value"));
  if ( WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    IPAddress ip = WiFi.localIP();
    response_message += getTableRow2Col(F("WLAN IP"), formatIPAsString(ip));
    response_message += getTableRow2Col(F("WLAN MAC"), WiFi.macAddress());
    response_message += getTableRow2Col(F("WLAN SSID"), WiFi.SSID());
  }
    response_message += getTableRow2Col(F("WLAN RSSI"), String(WiFi.RSSI()));
  if ( WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    IPAddress softapip = WiFi.softAPIP();
    response_message += getTableRow2Col(F("AP IP"), formatIPAsString(softapip));
    response_message += getTableRow2Col(F("AP MAC"), WiFi.softAPmacAddress());
    response_message += getTableRow2Col(F("AP SSID"), String(ap_ssid));
    response_message += getTableRow2Col(F("AP connections"),String(WiFi.softAPgetStationNum()));
  }

  // Make the uptime readable
  long upSecs = millis() / 1000;
  long upDays = upSecs / 86400;
  long upHours = (upSecs - (upDays * 86400)) / 3600;
  long upMins = (upSecs - (upDays * 86400) - (upHours * 3600)) / 60;
  upSecs = upSecs - (upDays * 86400) - (upHours * 3600) - (upMins * 60);
  String uptimeString = ""; uptimeString += upDays; uptimeString += F(" days, "); uptimeString += upHours, uptimeString += F(" hours, "); uptimeString += upMins; uptimeString += F(" mins, "); uptimeString += upSecs; uptimeString += F(" secs");

  response_message += getTableRow2Col(F("Uptime"), uptimeString);
  response_message += getTableRow2Col(F("Version"), SOFTWARE_VERSION);
  response_message += getTableRow2Col(F("Serial Number"), serialNumber);

  response_message += getTableFoot();

  #ifdef ARDUINO_ARCH_ESP8266
    float voltage = ((float)ESP.getVcc() / (float)913) + .005;
    // 895.21 corrects for external 100K pulldown on NodeMCU ADC pin
    // internal voltage divider seems to be about 48.25K / 15.6K
    // this gets to 1% accuracy 2.9-3.5V on test unit
    // varies by unit, 890-920?
    char dtostrfbuffer[15];
    dtostrf(voltage, 7, 2, dtostrfbuffer);
    String vccString = String(dtostrfbuffer);
  #endif
  
  // ESP8266 Info table
  response_message += getTableHead2Col(F("Platform Information"), F("Name"), F("Value"));
  #ifdef ARDUINO_ARCH_ESP8266
    response_message += getTableRow2Col(F("Architecture"), F("ESP8266"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("Architecture"), F("ESP32"));
    response_message += getTableRow2Col(F("Chip revision"), String(ESP.getChipRevision()));
  #endif
  response_message += getTableRow2Col(F("CPU Freqency (MHz)"), String(ESP.getCpuFreqMHz()));
  if (useRTC) {
    getRTCTime();
    String RTCTime= String(Month)+"/"+String(Day, DEC)+"/"+String(Year, DEC)+" "+String(Hour, DEC)+":"+String(Minute, DEC)+":"+String(Second, DEC);
    response_message += getTableRow2Col(F("RTC Time"), RTCTime);
    response_message += getTableRow2Col(F("RTC Temp"), String(getRTCTemp(), 2));
  }
  String datetime = String(__DATE__) + ", " + String(__TIME__) +F(" EST");
  response_message += getTableRow2Col(F("Sketch compiled"), datetime);
  response_message += getTableRow2Col(F("Build notes"), BUILD_NOTES);
  response_message += getTableRow2Col(F("Sketch size"), formatBytes(ESP.getSketchSize()));
  String ota = formatBytes(ESP.getFreeSketchSpace());
  if ( largeFlash ) { ota += F(" (OTA update capable)"); }
  response_message += getTableRow2Col(F("Free sketch size"), ota);
  response_message += getTableRow2Col(F("SDK version"), String(ESP.getSdkVersion()));
  
  #ifdef ARDUINO_ARCH_ESP8266
    response_message += getTableRow2Col(F("Free heap"), formatBytes(ESP.getFreeHeap()));
    response_message += getTableRow2Col(F("Heap fragmentation"), String(ESP.getHeapFragmentation())+" %");
    response_message += getTableRow2Col(F("Stack low watermark"), formatBytes(ESP.getFreeContStack()));
  
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    response_message += getTableRow2Col(F("SPIFFS size"), formatBytes(fs_info.totalBytes));
    response_message += getTableRow2Col(F("SPIFFS used"), formatBytes(fs_info.usedBytes));
    response_message += getTableRow2Col(F("SPIFFS block size"), formatBytes(fs_info.blockSize));
    response_message += getTableRow2Col(F("SPIFFS page size"), formatBytes(fs_info.pageSize));
    response_message += getTableRow2Col(F("SPIFFS max open files"), String(fs_info.maxOpenFiles));
    
    response_message += getTableRow2Col(F("ESP Core version"), String(ESP.getCoreVersion()));
    response_message += getTableRow2Col(F("Chip ID"), String(ESP.getChipId()));
    response_message += getTableRow2Col(F("Flash Chip ID"), "0x"+String(ESP.getFlashChipId(),HEX));
    response_message += getTableRow2Col(F("Flash size"), formatBytes(ESP.getFlashChipRealSize()));
    extern SpiFlashChip *flashchip;
/* typedef struct{
        uint32  deviceId;
        uint32  chip_size;    // chip size in byte
        uint32  block_size;
        uint32  sector_size;
        uint32  page_size;
        uint32  status_mask;
   } SpiFlashChip;
*/
    response_message += getTableRow2Col(F("Flash block size"), formatBytes(flashchip->block_size));
    response_message += getTableRow2Col(F("Flash sector size"), formatBytes(flashchip->sector_size));
    response_message += getTableRow2Col(F("Flash page size"), formatBytes(flashchip->page_size));

    
    response_message += getTableRow2Col(F("Last reset reason"), String(ESP.getResetReason()));
    response_message += getTableRow2Col(F("Vcc"), vccString);
    float mbv = 0.0;
    if (model == "PS-MPPT") {
      MBus_get_float(8, mbv);  // TODO make this universal
    } else if (model == "PS-PWM") {
      MBus_get_float(6, mbv);
    }
    response_message += getTableRow2Col(F("Meterbus Voltage"), String(mbv) + " V");
  #endif

  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("Internal total heap"), String(ESP.getHeapSize()));
    response_message += getTableRow2Col(F("Internal free heap"), String(ESP.getFreeHeap()));
    response_message += getTableRow2Col(F("Internal min free heap"), String(ESP.getMinFreeHeap()));
    response_message += getTableRow2Col(F("SPIRAM total heap"), String(ESP.getPsramSize()));
    response_message += getTableRow2Col(F("SPIRAM free heap"), String(ESP.getFreePsram()));
    response_message += getTableRow2Col(F("SPIRAM min free heap"), String(ESP.getMinFreePsram()));
    response_message += getTableRow2Col(F("Flash chip size"), "0x"+String(ESP.getFlashChipSize(),HEX));
    response_message += getTableRow2Col(F("Flash chip speed"), String(ESP.getFlashChipSpeed()));
    response_message += getTableRow2Col(F("Last reset reason CPU 0"), get_reset_reason(0));
    response_message += getTableRow2Col(F("Last reset reason CPU 1"), get_reset_reason(1));
    int a0_adc = analogRead(36);  // aka A0, ADC1
    a0_adc += analogRead(36);
    float vref = 1.1;
    analogSetAttenuation(ADC_11db);
    #define ATTEN 3.194  // 11 db (empirical)
    float vcc = (a0_adc/4095.0) * ATTEN * vref ;
    if (vcc < 3.6 && vcc > 2.5) {
      response_message += getTableRow2Col(F("Vcc"), String(vcc));
    }
    float mbv = 0.0;
    if (model == "PS-MPPT") {
      MBus_get_float(8, mbv);  // TODO make this universal
    } else if (model == "PS-PWM") {
      MBus_get_float(6, mbv);
    }
    response_message += getTableRow2Col(F("Meterbus Voltage"), String(mbv) + " V");
  
  
  #endif
  response_message += getTableFoot();

  response_message += getHTMLFoot();

  server.send(200, F("text/html"), response_message);
}

void statusPageHandler () {
/*  
 *   Returns a page of basic controller status.
 */
  #if DEBUG_ON>0
    debugMsg(F("Entering /status page."));
  #endif
  fullReg reg;
  int result;
  String response_message;
    
  checkController();

  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<div class=\"controller\"><h3>");
  if (noController) {
    response_message += F("No Controller");
  } else {
    response_message += model;
    if (controllerNeedsReset()) response_message += F(" (Controller needs restart)");
  }
  response_message += F("</h3></div>"); 

// 56,57 alarms
// 47 load fault
// 34 array fault
// TODO make this configurable

  String registers[] = {F("adc_vb_f_1m"), F("adc_ib_f_1m"), F("vb_ref"), F("T_batt"), F("adc_va"), F("adc_ia"), \
                        F("adc_il"), F("Ahc_daily"), F("Ahl_daily"), F("time_ab_daily"),F("time_fl_daily"), \
                        F("time_eq_daily"), F("Vb_min_daily"), F("Vb_max_daily"), F("adc_vl"), F("Va_max_daily") };
  String labels[] = {"", "", "Target: ", "", "", "", "", "", "", "Abs: ", "Float: ", "Eq: ", "Min: ", "Max: ", "", "Max: "};

  response_message += F("<div class=\"controller\"><img src=\"");
  if ( model == "") {
    response_message += F("Nocontroller.png");    
  } else {
    response_message += model + ".png";
  }
  response_message += F("\" alt=\"controller\">");
  float adc_pa = 0;
  for ( int i=0 ; i < 16 ; i++) {
    if ( registers[i] == F("adc_va") || registers[i] == F("adc_ia") ) { //handle power calculation
      int addy;
      float vary;
      addy = findAddrByVar(registers[i]);
      MBus_get_float(addy, vary);
      if (adc_pa) {
        adc_pa = adc_pa * vary;
        response_message += F("<div id=\"adc_pa\">");
        response_message += String(adc_pa);
        response_message += F(" W</div>");
      } else {
        adc_pa = vary;
      }
    } 
    response_message += getStatusItem(registers[i], labels[i]);  // items are placed on graphic via local.css
  }
  response_message += F("</div>"); 

  response_message += getTableHead2Col(F("Battery"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 24);  // Batt V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 36);  // Batt target V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 25);  // Batt A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 27);  // Batt Temp
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 65);  // Batt minv
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 66);  // Batt maxv
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  response_message += getTableFoot();
 
  response_message += getTableHead2Col(F("Charging"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 67);  // Ah charge daily
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 73);  // Time abs
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  result = mbGetFullReg(reg, 75);  // Time float
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  result = mbGetFullReg(reg, 74);  // Time eq
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Solar"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 19);  // Array V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  float aVolts = reg.value.toFloat();
  result = mbGetFullReg(reg, 17);  // Array A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  float aWatts = reg.value.toFloat() * aVolts ;
  response_message += getTableRow2Col(F("Array Watts"), String(aWatts) + " W");  
  result = mbGetFullReg(reg, 76);  // Array MaxV
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Load"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 20);  // Load V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 22);  // Load A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 68);  // Load Ah
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  response_message += getTableFoot();

  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);

}

void allregsPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /allregs page."));
  #endif
  String response_message;
  response_message.reserve(8500);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += getTableHead2Col(model+" Registers", F("Register"), F("Value"));
  int response;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  bool eof = false;
  for ( int row = 1; !eof ; row++ ) {
    if (mbRegAddr[row] >= mbRegMax) { eof = true; }    
    String reginfo = mbRegDesc[row] + " [" + String(mbRegAddr[row]) + "]" + " (" + mbRegVar[row] + ")";
    String unit = " " + mbRegUnitName[mbRegUnit[row]];
    #if DEBUG_ON>3
      debugMsgContinue(F("processing modbus register "));
      debugMsg(String(mbRegAddr[row]));
    #endif
    switch (mbRegType[row]) {
      case f16:     MBus_get_float(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);
                    break;
      case n10:     MBus_get_n10(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);        
                    break;
      case dn10:    MBus_get_dn10(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);        
                    break;
      case sigint:  MBus_get_int(mbRegAddr[row], foo_sint);
                    response_message += getTableRow2Col(reginfo, String(foo_sint) + unit);                
                    break;
      case usigint: MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, String(foo_int) + unit);        
                    break;
      case dint:    MBus_get_uint32(mbRegAddr[row], foo_dint);
                    response_message += getTableRow2Col(reginfo, String(foo_dint) + unit);        
                    break;
      case bitfield: MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_int, HEX) + unit);        
                    break;
      case dbitfield: MBus_get_uint32(mbRegAddr[row], foo_dint);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_dint, HEX) + unit);        
                    break;
      case bcd:     MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_int, HEX) + unit);        
                    break;
      // TODO additional types (n673 onward)             
      default:           ;
    }
    delay(1); // brief pause between reading registers works best
  }
  response_message += getTableFoot();

  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}


void allcoilsPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /allcoils page."));
  #endif
  String response_message;
  response_message.reserve(3000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  int response;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  bool eof = false;

  response_message += getTableHead2Col(model+" Coils", F("Coil"), F("State"));
  eof = false;
  bool state;
  String coilInfo;
  for ( int row = 1; row <= COIL_ROWS && !eof ; row++ ) {
    if (mbCoilAddr[row] >= mbCoilMax) { eof = true; }
    #if DEBUG_ON>3
      debugMsgContinue(F("processing modbus coil "));
      debugMsg(String(mbCoilAddr[row]));
    #endif
    MBus_get_coil(mbCoilAddr[row], state);
    coilInfo = mbCoilDesc[row] + " [" + String(mbCoilAddr[row]) + "]" + " (" + mbCoilVar[row] + ")";
    response_message += getTableRow2Col(coilInfo, String(state));
    delay(1);
  }
  response_message += getTableFoot();
  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

/**
   WLAN page allows users to set the WiFi credentials
*/
void wlanPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /wlan_config page."));
  #endif

  // Check if there are any GET parameters, if there are, we are configuring
  if (server.hasArg(F("ssid")))
  {
    WiFi.persistent(true);
    #if DEBUG_ON>0
      debugMsgContinue(F("New SSID entered: \""));
      debugMsg(String(server.arg("ssid").c_str())+"\"");
    #endif
    if (server.hasArg(F("password")))
    {
      #if DEBUG_ON>3
      debugMsg(F("Configuring WiFi"));
      debugMsgContinue(F("SSID:"));
      debugMsg(server.arg(F("ssid")));
      debugMsgContinue(F("PASSWORD:"));
      #endif
      #if DEBUG_ON>5
      debugMsgContinue(server.arg(F("password")));
      #endif
      #if DEBUG_ON>3
      debugMsg("");
      #endif

      WiFi.begin(server.arg(F("ssid")).c_str(), server.arg(F("password")).c_str());
    }
    else
    {
      WiFi.begin(server.arg(F("ssid")).c_str());
      #if DEBUG_ON>3
      debugMsg(F("Connect WiFi"));
      debugMsg(F("SSID:"));
      debugMsg(server.arg(F("ssid")));
      #endif
    }
    WiFi.persistent(false);

    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 30) // try for 15 seconds
    {
      delay(500);
      server.handleClient();                            // for web server
      i++;
      #if DEBUG_ON>0
      debugMsgContinue(".");
      #endif
    }

    if (WiFi.status() == WL_CONNECTED) {
    storeCredentialsInEEPROM(server.arg(F("ssid")), server.arg(F("password")));
    #if DEBUG_ON>0
      debugMsg("");
      debugMsg(F("WiFi connected"));
      debugMsgContinue(F("IP address: "));
      debugMsg(formatIPAsString(WiFi.localIP()));
      debugMsgContinue(F("SoftAP IP address: "));
      debugMsg(formatIPAsString(WiFi.softAPIP()));
    #endif
    delay(50);
    reboot();
    } else {
      #if DEBUG_ON>0
        debugMsg("");
        debugMsg(F("WiFi connect failed."));
      #endif
    }
  }

  String esid = getSSIDFromEEPROM();

  String response_message;
  response_message.reserve(3000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  // form header
  response_message += getFormHead(F("Set Configuration"));

  #if DEBUG_ON>3
    debugMsg("Starting WiFi scan.");
  #endif
  // Get number of visible access points
  int ap_count = WiFi.scanNetworks();

  response_message += getDropDownHeader(F("WiFi:"), F("ssid"), true);

  if (ap_count == 0)
  {
    response_message += getDropDownOption(F("-1"), F("No wifi found"), true);
  }
  else
  {
    // Show access points
    for (uint8_t ap_idx = 0; ap_idx < ap_count; ap_idx++)
    {
      String ssid = String(WiFi.SSID(ap_idx));
      String wlanId = String(WiFi.SSID(ap_idx));
      #ifdef ARDUINO_ARCH_ESP8266      
        (WiFi.encryptionType(ap_idx) == ENC_TYPE_NONE) ? wlanId += "" : wlanId += F(" (requires password)");
      #endif 
      #ifdef ARDUINO_ARCH_ESP32
        (WiFi.encryptionType(ap_idx) == WIFI_AUTH_OPEN) ? wlanId += "" : wlanId += F(" (requires password)");
      #endif 
      wlanId += F(" (RSSI: ");
      wlanId += String(WiFi.RSSI(ap_idx));
      wlanId += F(")");

      #if DEBUG_ON>2
      debugMsgContinue(F("Found ssid: "));
      debugMsg(WiFi.SSID(ap_idx));
      if ((esid == ssid)) {
        debugMsg(F("IsCurrent: Y"));
      } else {
        debugMsg(F("IsCurrent: N"));
      }
      #endif
      
      response_message += getDropDownOption(ssid, wlanId, (esid == ssid));
    }

    response_message += getDropDownFooter();

    response_message += getTextInput(F("WiFi password (if required)"), F("password"), "", false);
    response_message += getSubmitButton(F("Set"));

    response_message += getFormFoot();
  }

  response_message += getHTMLFoot();

  server.send(200, F("text/html"), response_message);

}

/**
   Utility functions
*/
void utilityPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /utility page."));
  #endif

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<br/><br/><div class=\"container\" role=\"secondary\"><br/>");
  response_message += F("<p><hr><h3>Utility Functions</h3>");
  response_message += F("<font size=\"4\">");

  response_message += F("<hr><a href=\"/wlan_config\">Wireless settings</a>");
  if (useRTC) {
    response_message += F("<hr><a href=\"/setTime\">Set time</a>");
  }
  response_message += F("<hr><a href=\"/documentation.htm\">Documentation</a>");
  response_message += F("<hr><a href=\"/edit\">File edit/view/upload (quirky)</a>");
  response_message += F("<hr><a href=\"/allregs\">Show all registers</a>");
  response_message += F("<hr><a href=\"/allcoils\">Show all coils</a>");
  response_message += F("<hr><a href=\"/rest?json={%22addr%22:255,%22cmd%22:");
  response_message += F("%22writeSingleCoil%22,%22valu%22:%22on%22,%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22,%22back%22:%22true%22}\">Restart solar controller</a>");

  response_message += F("<hr><a href=\"/rest?json={%22addr%22:254,%22cmd%22:");
  response_message += F("%22writeSingleCoil%22,%22valu%22:%22on%22,%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22,%22back%22:%22true%22}\">Reset solar controller to factory defaults</a>");

  response_message += F("<hr><a href=\"/getfile\">Check controller and reread files</a>");
  response_message += F("<hr><a href=\"/reset\">Restart WLAN module</a>");
  response_message += F("<hr><a href=\"/resetall\">Clear config and restart WLAN module</a>");
  if ( largeFlash ) {
    response_message += F("<hr><a href=\"/update\">Update WLAN module firmware</a>");
  }

  response_message += F("</font></div>");
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

/**
   Reset the ESP card
*/
void getfilePageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /getfile page."));
  #endif

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Checking controller and rereading definition file.</strong>");
  checkController();
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
  getFile(model);
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 3000);</script>");  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}


/**
   Reset the EEPROM and stored values
*/
void resetAllPageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /resetall page."));
  #endif

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Success!</strong> Reset done.</div>");
  response_message += F("<div class=\"alert alert-success fade in\">Attempting reboot, but power cycle if needed. ");
  response_message += F("You will then need to connect to the <b>"); 
  response_message += String(ap_ssid);
  response_message += F("</b> SSID and open <b>http://192.168.4.1</b> in a web browser to reconfigure.</div></div>");
  response_message += getHTMLFoot();
  resetEEPROM();
  system_restore(); // this wipes ESP saved wifi stuff
  server.send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
    server.handleClient();
    delay(1);    // wait to deliver response
    yield();
  }
  reboot();
}

/**
   Reset the ESP card
*/
void resetPageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /reset page."));
  #endif

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Attempting a restart.</strong>");
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 12000);</script>");  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
    server.handleClient();
    delay(1);    // wait to deliver response
    yield();
  }
  reboot();
}

void setTimePageHandler() {
 //   Page to set RTC.
  #if DEBUG_ON>0
    debugMsg(F("Entering /setTime page."));
  #endif
  checkController();
  String response_message, inputvar;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"controller\"><h3>");
  if (noController) {
    response_message += F("No Controller");
  } else {
    response_message += model;
    if (controllerNeedsReset()) response_message += F(" (Controller needs restart)");
  }
  response_message += F("</h3></div>"); 

//  response_message += F("<center><img src=\"setTime.png\"></center>");
  response_message += getTableHead2Col(F("Set Time"), F("Unit"), F("Value")); 
  getRTCTime();
  response_message += getJsButton(F("Set time and date"), "setRtcTime()");

  String RTCTime= String(Month)+"/"+String(Day, DEC)+"/"+String(Year, DEC)+" "+String(Hour, DEC)+":"+String(Minute, DEC)+":"+String(Second, DEC);
  response_message += getTableRow2Col(F("Current Time"), RTCTime);
 // response_message += getTableRow2Col(F("Aging offset"), String(getAgingOffset()));
  
  inputvar = F("<input type=\"number\" id=\"offset\" size=\"9\" value=\"");
  inputvar += String(getAgingOffset());
  inputvar += F("\" onchange=\"setAging(this.value, 'offset')\">&nbsp");
  response_message += getTableRow2Col("Aging offset (-127 to 127, higher is slower)", inputvar);
  
  response_message += getTableRow2Col(F("Last set"), String(geteeUnixTime()));
  response_message += getTableRow2Col(F("Current"), String(getUnixTime()));

/*
  inputvar = F("<input type=\"number\" id=\"Year\" size=\"9\" value=\"");
  inputvar += String(Year);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Year", inputvar );

  inputvar = F("<input type=\"number\" id=\"Month\" size=\"9\" value=\"");
  inputvar += String(Month);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Month", inputvar );

  inputvar = F("<input type=\"number\" id=\"Day\" size=\"9\" value=\"");
  inputvar += String(Day);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Day", inputvar );

  inputvar = F("<input type=\"number\" id=\"Hour\" size=\"9\" value=\"");
  inputvar += String(Hour);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Hour", inputvar );

  inputvar = F("<input type=\"number\" id=\"Minute\" size=\"9\" value=\"");
  inputvar += String(Minute);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Minute", inputvar );

  inputvar = F("<input type=\"number\" id=\"Second\" size=\"9\" value=\"");
  inputvar += String(Second);
  inputvar += "\" onchange=\"('foo', this.value, '";
  inputvar += "5";
  inputvar += "')\">&nbsp";
  response_message += getTableRow2Col("Second", inputvar );
*/
  response_message += getTableFoot();

  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}
