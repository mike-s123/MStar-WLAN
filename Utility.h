// ----------------------------------------------------------------------------------------------------
// ----------------------------------------- Utility functions ----------------------------------------
// ----------------------------------------------------------------------------------------------------


void setWlanLED(boolean newState) {
  digitalWrite(WLAN_PIN, newState);
}

String formatIPAsString(IPAddress ip) {
  return String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
}

#if DEBUG_ON>0                              // needed if debugging
  void debugMsg(String msg) {
    #ifdef ARDUINO_ARCH_ESP8266
      logger.println(msg);
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      Serial.println(msg);
    #endif
  }
  
  void debugMsgContinue(String msg) {
    #ifdef ARDUINO_ARCH_ESP8266
    logger.print(msg);
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      Serial.print(msg);
    #endif
  }
  
  void setupDebug() {
    #ifdef ARDUINO_ARCH_ESP8266
      logger.begin(BAUD_LOGGER);
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      Serial.begin(BAUD_LOGGER);
    #endif
    debugMsg("");
    debugMsg(F("Starting debug session"));
  }
#else                                       // maybe serial passthrough will work someday
  void setupPassthru() {
/*
 * Try to pass serial from USB side (softserial because we swapped UART pins)
 * through to the controller, half duplex Modbus.
 */
//    cSerial->begin(9600);
  }
  
  void rxEnable(bool state); // forward declaration
  
  void serialPassthrough() {  // this connects Serial (controller) to cSerial (USB)
//work in progress...
/*    static long int txdTimer;
    
    while ( (cSerial->available() > 0) ) {
      rxEnable(false);
      txdTimer = millis();
      cSerial->write(cSerial->read());  //echo back for testing
//      yield();
    }
    
    while (Serial.available() >0 ) {
      cSerial->write(Serial.read());
//      yield();
    }
    cSerial->flush();
    if ( (millis() - txdTimer) >= 1 ) {   // little over 1 character time
      rxEnable(true); 
    }
*/

return;  
  }
#endif                                            // end serial passthrough

int getDecInt(String value) {
/*  
 *   From a String of decimal, hex, or octal, 
 *   return an int.
 */
  char *endptr;
  #if DEBUG_ON>3
    debugMsg("getAddress of "+value);
  #endif
  return strtol(value.c_str(),&endptr,0) ;  // default decimal, 0x = hex, 0=octal
}

inline String secToMin(String seconds) {
  return String(seconds.toInt()/60);
}

/*
 *  This stores WLAN credentials in the first slot [0]
 */
void storeWLANsInEEPROM(String qsid, String qpass, int idx=0) {
  #if DEBUG_ON>2
  debugMsg("writing eeprom "+String(idx)+" ssid " + qsid );
  #endif
  if (idx > 3) return;
  wlanRead = false;                   // now needs to be re-read
  for (int i = 0; i < 32; i++) {
    if (i < qsid.length()) {
      EEPROM.write(eeWLANSSID + i + (32*idx), qsid[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote: " + String(qsid[i]));
      #endif
    } else {
      EEPROM.write(eeWLANSSID + i + (32*idx), 0);
    }
  }
  #if DEBUG_ON>2
  debugMsg("writing eeprom "+String(idx)+" pass " + qpass);
  #endif
  for (int i = 0; i < 32; i++) {
    if ( i < qpass.length()) {
      EEPROM.write(eeWLANPASS + (32*idx) + i, qpass[i]);
      #if DEBUG_ON>8
      debugMsg("Wrote: " + String(qpass[i]));
      #endif
    } else {
      EEPROM.write(eeWLANPASS + (32*idx) + i, 0);
    }
  }
  EEPROM.commit();
}

void getWLANsFromEEPROM() {
  if (wlanRead) return;
  byte readByte;
  for (int j = 0; j<=3; j++){
    esid[j] = "";
    int i;
    for (i = 0; i < 32; i++) {
      readByte = EEPROM.read(eeWLANSSID + i+(j*32));
      if (readByte == 0) {
        break;
      } else if ((readByte < 32) || (readByte > 126)) {
        continue;
      }
      esid[j] += char(readByte);
    }
    #if DEBUG_ON>2
    debugMsg("Read SSID " + String(j) +":" + esid[j]);
    #endif
    wlanRead = true;
    wlanSet = false;
  }
  
  for (int j = 0; j<=3; j++){
    epass[j] = "";
    for (int i = 0; i < 32; i++) {
      readByte = EEPROM.read(eeWLANPASS + i + (j*32));
      if (readByte == 0) {
        break;
      } else if ((readByte < 32) || (readByte > 126 )) {
        continue;
      }
      epass[j] += char(readByte);
    }
    #if DEBUG_ON>2
      debugMsgContinue("Read password:");
    #endif
    #if DEBUG_ON>8                            // only show password debug level 6+
      debugMsgContinue(epass[j]);
    #endif
    #if DEBUG_ON>2
      debugMsg("");
    #endif
  }
}

void storeModelInEEPROM(String model) {
  #if DEBUG_ON>2
  debugMsg("writing eeprom model " + model +" ("+String(model.length())+")");
  #endif
  for (int i = 0; i < 16; i++)
  {
    if (i < model.length()) {
      EEPROM.write(i+eeModel, model[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote: " + String(model[i]));
      #endif
    } else {
      EEPROM.write(i+eeModel, 0);
    }
  }
  EEPROM.commit();
}

String getModelFromEEPROM() {
  String model = "";
  byte readByte;
  for (int i = 0; i < 16; i++)
  {
    readByte = EEPROM.read(i+eeModel);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte > 126)) {
      continue;
    }
    model += char(readByte);
  }
  #if DEBUG_ON>2
    debugMsg("EEPROM read model: " + model);
  #endif
  return model;
}

void storeNtpServerInEEPROM(String server) {
  #if DEBUG_ON>2
  debugMsg("eeprom writing NtpServer:" + server + " ("+String(server.length())+")");
  #endif
  for (int i = 0; i < 32; i++)
  {
    if (i < server.length()) {
      EEPROM.write(i+eeNtpServer, server[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote:" + String(server[i]));
      #endif
    } else {
      EEPROM.write(i+eeNtpServer, 0);
    }
  }
  EEPROM.commit();
}

String getNtpServerFromEEPROM() {
  String server = "";
  byte readByte;
  for (int i = 0; i < 32; i++)
  {
    readByte = EEPROM.read(i+eeNtpServer);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte > 126)) {
      continue;
    }
    server += char(readByte);
  }
  #if DEBUG_ON>2
    debugMsg("eeprom read NtpServer: " + server);
  #endif
  return server;
}

void storeNtpTZInEEPROM(String tz) {
  #if DEBUG_ON>2
  debugMsg("eeprom writing NtpTZ:" + tz + " ("+String(tz.length())+")");
  #endif
  for (int i = 0; i < 32; i++)
  {
    if (i < tz.length()) {
      EEPROM.write(i+eeNtpTZ, tz[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote:" + String(tz[i]));
      #endif
    } else {
      EEPROM.write(i+eeNtpTZ, 0);
    }
  }
  EEPROM.commit();
}

String getNtpTZFromEEPROM() {
  String tz = "";
  byte readByte;
  for (int i = 0; i < 32; i++)
  {
    readByte = EEPROM.read(i+eeNtpTZ);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte > 126)) {
      continue;
    }
    tz += char(readByte);
  }
  #if DEBUG_ON>2
    debugMsg("eeprom read NtpTZ:" + tz);
  #endif
  return tz;
}

void storeNtpPollInEEPROM(unsigned short int poll) {
  #if DEBUG_ON>2
  debugMsg("eeprom writing NtpPoll:" + String(poll) + " (2)");
  #endif
  EEPROM.write(eeNtpPoll, (poll >> 8));
  EEPROM.write(eeNtpPoll+1, (poll & 0xff));
  #if DEBUG_ON>4
  debugMsg("Wrote:" + String(poll));
  #endif
  EEPROM.commit();
}

unsigned short int getNtpPollFromEEPROM() {
  unsigned short int poll = 0;
    poll = EEPROM.read(eeNtpPoll) << 8;
    poll += EEPROM.read(1 + eeNtpPoll);
  #if DEBUG_ON>2
    debugMsg("eeprom read NtpPoll: " + String(poll));
  #endif
  return poll;
}

void wipeEEPROM() {
  #if DEBUG_ON>0
    debugMsg(F("Wiping EEPROM."));
  #endif  
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 255);
  }
  EEPROM.commit();
}

void resetEEPROM() {
  wipeEEPROM();
  #if DEBUG_ON>0
    debugMsg(F("Resetting EEPROM."));
  #endif
  for (int i = 0; i<=3 ; i++) {
    storeWLANsInEEPROM("", "", i);
  }
  storeModelInEEPROM(F("PS-PWM"));
  storeNtpServerInEEPROM(F(NTP_DEFAULT_SERVER));
  storeNtpPollInEEPROM(NTP_DEFAULT_INTERVAL);
  storeNtpTZInEEPROM(F(NTP_DEFAULT_TZ));
  String chkstr = F(EEPROM_SIG);
  for (int i = 0; i<=3 ; i++) {
    EEPROM.write(i + EEPROM_SIZE -4, chkstr[i]);
    #if DEBUG_ON>4
      debugMsg("Wrote chkstr: " + chkstr[i]);
    #endif
  }
  EEPROM.commit();
} 

String checkEEPROM() {  
/* 
 *  Check if valid, reset if not
 */
  #if DEBUG_ON>0
    debugMsgContinue(F("Check ESP EEPROM..."));
  #endif    
  String chkstr = "";
  for (int i = EEPROM_SIZE - 4; i < EEPROM_SIZE; i++) {
    chkstr += char(EEPROM.read(i));
  }
  if (chkstr != F(EEPROM_SIG)) {
    #if DEBUG_ON>0
      debugMsg(F("invalid, resetting"));
    #endif  
    resetEEPROM();      
  } else {
  #if DEBUG_ON>0
    debugMsg(F("valid"));
  #endif  
  }
  return chkstr;
}

void startAP(const char* ssid, const char* password) {
/*
 * This starts AP only, probably because we had no stored STA info.
 */
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif
  WiFi.mode(WIFI_AP);
  #if DEBUG_ON>0
    debugMsgContinue(F("Starting AP, SSID \""));
    debugMsgContinue(String(ssid));
    debugMsgContinue(F("\", pass \""));
    debugMsgContinue(String(password));
    debugMsg(F("\""));
  #endif  
  if (strlen(password) > 0) {
    WiFi.softAP(ssid, password, 6, false, 8);
  } else {
    WiFi.softAP(ssid);
  }
}


boolean connectToWLAN(const char* ssid = "", const char* password = "") {
/*
 *  Try to connect as a station with the given credentials. Give up after a while.
 *   if we can't get in. Returns connected status.
 *   Station only, because AP_STA results in frequent disconnects (~5 minute intervals).
*/
  int retries = 0;
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif

//  WiFi.persistent(true);
  if (strlen(ssid)>0) {         // here we try to connect using the info passed to us
    wlan_count++ ;
    if (password && strlen(password) > 0 ) {    
        #if DEBUG_ON>3
          debugMsgContinue(F("wifiMulti adding SSID:"));
          debugMsg(ssid);
        #endif
        #if DEBUG_ON>8
          debugMsgContinue(F("wifiMulti   with pass:"));
          debugMsg(password);
        #endif
      wifiMulti.addAP(ssid, password);        //esid.c_str()?
    } else {
        wifiMulti.addAP(ssid);
    }
  } else {                        // if we weren't given info, look it up
    getWLANsFromEEPROM();
    int i;
    for (i = 0; i<=3; i++) {
      if (esid[i] != "") {
        if ( epass[i] != "" ) {   // got both ssid and pass
//          if (!wifiMulti.existsAP(esid[i].c_str(), epass[i].c_str()) ) { // skip if already there ESP8266 only
            if (!wlanSet) { // skip if already there
            #if DEBUG_ON>3
              debugMsgContinue(F("wifiMulti adding SSID from EEPROM:"));
              debugMsg(esid[i]);
              debugMsgContinue(F("wifiMulti   with pass from EEPROM:"));
              #if DEBUG_ON>8
                debugMsgContinue(epass[i]);
              #endif
              debugMsg("");
            #endif
            wifiMulti.addAP(esid[i].c_str(), epass[i].c_str());
            wlan_count++ ;
          }
        } else {  // only ssid, no pass
//          if (!wifiMulti.existsAP(esid[i].c_str())) {                  // skip if already there ESP8266 only
            if (!wlanSet) {                  // skip if already there
            #if DEBUG_ON>3
              debugMsgContinue(F("wifiMulti adding SSID from EEPROM:"));
              debugMsg(esid[i]);
            #endif
            wifiMulti.addAP(esid[i].c_str());
            wlan_count++ ;
          }
        }
      }
    }  // done getting WLANs
    wlanSet = true;
  }  // else, didn't get passed WLAN info
  
  #ifdef WIFI_MODE_AP_STA           // mode controlled by #define
    WiFi.mode(WIFI_AP_STA);
    if (strlen(ap_password) > 0) {
      WiFi.softAP(ap_ssid, ap_password, 6, false, 8);
    } else {
      WiFi.softAP(ap_ssid);
    }
  #else
    WiFi.mode(WIFI_STA);
    #if DEBUG_ON>0
      debugMsg(F("WLAN changing to station mode."));
    #endif
  #endif
  
  if (wlan_count) {    // if we have WLANs configured for station mode, try to connect
    #if DEBUG_ON>0
      if (wlan_count) {
        debugMsgContinue(F("WLAN connecting ("));
        debugMsgContinue(String(wlan_count));
        debugMsgContinue(")");
      }  
    #endif

    #ifdef ARDUINO_ARCH_ESP32
      WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));  // test, force DHCP
    #endif
    while ( wifiMulti.run() != WL_CONNECTED && wlan_count ) {
      delay(500);
      #if DEBUG_ON>0
        debugMsgContinue(".");
      #endif
      retries++;
      if (retries > 20) {   // try for 10  seconds
        #if DEBUG_ON>0
          debugMsg("");
          debugMsg(F("Failed to connect"));
        #endif      
        return false;
      }
    }
    #if DEBUG_ON>0
      if (WiFi.isConnected()) {
        debugMsg("");
        debugMsg(F("WLAN connected"));
      }
    #endif
  } else {  // no wlan_count
    #if DEBUG_ON>0
      debugMsg(F("No WLANs configured"));
    #endif
    return false;      
  }
  return true;
}

boolean tryWLAN() {  // simply tries to connect to configured WLANs
  int retries=0;
  #ifndef WIFI_MODE_AP_STA                       // we're in AP mode, switch to try
    #if DEBUG_ON>0
      debugMsg(F("WLAN switching to STA"));
    #endif
    WiFi.mode(WIFI_STA);
  #endif

  #if DEBUG_ON>0
      debugMsgContinue(F("Trying to connect to WLAN ("));
      debugMsgContinue(String(wlan_count));
      debugMsgContinue(")");  
  #endif
  while ( wifiMulti.run() != WL_CONNECTED ) {
    delay(500);
    #if DEBUG_ON>0
      debugMsgContinue(".");
    #endif
    retries++;
    if (retries > 20) {   // try for 10  seconds
      #if DEBUG_ON>0
      debugMsg("");
      debugMsg(F("Failed to connect"));
      #endif
      #ifndef WIFI_MODE_AP_STA                       // go back to AP mode so users can connect
        #if DEBUG_ON>0
        debugMsg(F("WLAN switching back to AP"));
        #endif
      startAP(ap_ssid, ap_password);
      #endif
      return false;
    }
  }
  #if DEBUG_ON>0
    debugMsg("");
    IPAddress ip = WiFi.localIP();
    debugMsgContinue(F("WLAN IP address:"));
    debugMsg(formatIPAsString(ip));
    debugMsg("Connected to:" + String(WiFi.SSID()));
  #endif
  return true;
}


class IEEEf16  // Convert between float32 (IEEE754 Single precision binary32) and float16 (IEEE754 half precision binary16)
/*
 * This code posted by user Phernost on 
 * https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
 *
 * compress and decompress methods are made "inline" for performance
 * 
 * Public Domain - https://choosealicense.com/licenses/unlicense/
 */
{    union Bits
    {   float f;
        int32_t si;
        uint32_t ui;
    };

    static int const shift = 13;
    static int const shiftSign = 16;

    static int32_t const infN = 0x7F800000; // flt32 infinity
    static int32_t const maxN = 0x477FE000; // max flt16 normal as a flt32
    static int32_t const minN = 0x38800000; // min flt16 normal as a flt32
    static int32_t const signN = 0x80000000; // flt32 sign bit

    static int32_t const infC = infN >> shift;
    static int32_t const nanN = (infC + 1) << shift; // minimum flt16 nan as a flt32
    static int32_t const maxC = maxN >> shift;
    static int32_t const minC = minN >> shift;
    static int32_t const signC = signN >> shiftSign; // flt16 sign bit

    static int32_t const mulN = 0x52000000; // (1 << 23) / minN
    static int32_t const mulC = 0x33800000; // minN / (1 << (23 - shift))

    static int32_t const subC = 0x003FF; // max flt32 subnormal down shifted
    static int32_t const norC = 0x00400; // min flt32 normal down shifted

    static int32_t const maxD = infC - maxC - 1;
    static int32_t const minD = minC - subC - 1;

public:
    static uint16_t f16(float value)
    {
        Bits v, s;
        v.f = value;
        uint32_t sign = v.si & signN;
        v.si ^= sign;
        sign >>= shiftSign; // logical shift
        s.si = mulN;
        s.si = s.f * v.f; // correct subnormals
        v.si ^= (s.si ^ v.si) & -(minN > v.si);
        v.si ^= (infN ^ v.si) & -((infN > v.si) & (v.si > maxN));
        v.si ^= (nanN ^ v.si) & -((nanN > v.si) & (v.si > infN));
        v.ui >>= shift; // logical shift
        v.si ^= ((v.si - maxD) ^ v.si) & -(v.si > maxC);
        v.si ^= ((v.si - minD) ^ v.si) & -(v.si > subC);
        return v.ui | sign;
    }

    static float f32(uint16_t value)
    {
        Bits v;
        v.ui = value;
        int32_t sign = v.si & signC;
        v.si ^= sign;
        sign <<= shiftSign;
        v.si ^= ((v.si + minD) ^ v.si) & -(v.si > subC);
        v.si ^= ((v.si + maxD) ^ v.si) & -(v.si > maxC);
        Bits s;
        s.si = mulC;
        s.f *= v.si;
        int32_t mask = -(norC > v.si);
        v.si <<= shift;
        v.si ^= (s.si ^ v.si) & mask;
        v.si |= sign;
        return v.f;
    }
};  // need semicolon to end class definition

void reboot() {
  #if DEBUG_ON>0
    debugMsg(F("Doing ESP.restart()"));
  #endif  
  #ifdef ARDUINO_ARCH_ESP8266
    digitalWrite(SELF_RST_PIN, LOW); // this should do a hard reset.
    delay(50);
  #endif
  ESP.restart();         // if not, this should do a soft reboot.
}

#ifdef ARDUINO_ARCH_ESP32
  #include <rom/rtc.h>
  String get_reset_reason(int cpu)
  { 
    RESET_REASON reason;
    String reasonString;
    reason = rtc_get_reset_reason(cpu);
    switch ( reason)
    {
      case 1  : reasonString = F("Vbat power on reset");break;
      case 3  : reasonString = F("Software reset digital core");break;
      case 4  : reasonString = F("Legacy watch dog reset digital core");break;
      case 5  : reasonString = F("Deep Sleep reset digital core");break;
      case 6  : reasonString = F("Reset by SLC module, reset digital core");break;
      case 7  : reasonString = F("Timer Group0 Watch dog reset digital core");break;
      case 8  : reasonString = F("Timer Group1 Watch dog reset digital core");break;
      case 9  : reasonString = F("RTC Watch dog Reset digital core");break;
      case 10 : reasonString = F("Instrusion tested to reset CPU");break;
      case 11 : reasonString = F("Time Group reset CPU");break;
      case 12 : reasonString = F("Software reset CPU");break;
      case 13 : reasonString = F("RTC Watch dog Reset CPU");break;
      case 14 : reasonString = F("for APP CPU, reset by PRO CPU");break;
      case 15 : reasonString = F("Reset when the vdd voltage is not stable");break;
      case 16 : reasonString = F("RTC Watch dog reset digital core and rtc module");break;
      default : reasonString = F("unknown");
    }
    return reasonString;
  }
#endif

String getJsButton(String buttonText, String onClick); // fwd declaration

String promptReset() {
  String response_message = "";
  bool controllerNeedsReset (); // fwd declaration
  if (controllerNeedsReset()) {
    response_message = F("<div class=\"controller\">");
    response_message += F("<br><h3>Settings changed, controller needs restart.</h3>");
    response_message += getJsButton(F("Restart Controller"), F("restart_ctl()"));
    response_message = F("</div>");
    }
  return response_message;
}
