// ----------------------------------------------------------------------------------------------------
// ----------------------------------------- Utility functions ----------------------------------------
// ----------------------------------------------------------------------------------------------------


void rxEnable(bool state); // forward declarations
void checkController();
void psLog();

void debugMsgln(String msg, int level) {
  #ifdef DEBUG_ON
    if (level <= debug_level) {
      Serial.println(msg);
      if (sd_card_available && logFile) {
        if (needLogTime) logFile.print(myTZ.dateTime(ATOM)+" ");
        logFile.println(msg);
        needLogTime = true;
      } 
    } // level
  #endif
}

void debugMsg(String msg, int level) {
  #ifdef DEBUG_ON
    if (level <= debug_level) {
      Serial.print(msg);
      if (sd_card_available && logFile) {
        if (needLogTime) {
          logFile.print(myTZ.dateTime(ATOM)+" ");
          needLogTime = false;
        }
        logFile.print(msg);
      }
    } // level
  #endif
}

void setupDebug() {
  #ifdef DEBUG_ON
    Serial.begin(BAUD_LOGGER);
    #ifdef EZT_DEBUG
      setDebug(EZT_DEBUG,ezt_logFile);
    #endif
    debugMsgln("",1);
    debugMsgln(F("Starting debug session"),1);
  #endif
}

int psGetSn(); //fwd
int getSn() {
  checkController();
  if (!noController) {
    if ( model.startsWith("PS-")) {  // break out different controller families TODO more families
      return psGetSn();
    }
  }
}

void psGetCtlLogFileName(); //fwd dec
void getCtlLogFileName() {
  if (!noController) {
    if ( model.startsWith("PS-")) {  // break out different controller families TODO more families
      psGetCtlLogFileName();
    }
  }
}

void ctlLog() {
  checkController();
  if (!noController && sd_card_available && ctl_logFile) {
    if ( model.startsWith("PS-")) {  // break out different controller families TODO more families
      debugMsgln(F("Logging PS- controller"),3);
      psLog();
    }
  }
}

void PSopenLogFile(); // fwd dec
void refreshCtlLogFile(){
  if (!noController) {
    if ( model.startsWith("PS-")) {  // break out different controller families TODO more families
      PSopenLogFile();
    }
  }
}

void setupPassthru() {
/*
 * Try to pass serial from USB side (softserial because we swapped UART pins)
 * through to the controller, half duplex Modbus.
 */
//    cSerial->begin(9600);
}
  
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
} // serial passthrough

int getDecInt(String value) {
// From a String of decimal, hex, or octal, return an int.
  char *endptr;
  return strtol(value.c_str(),&endptr,0) ;  // default decimal, 0x=hex, O=octal
}

inline String secToMin(String seconds) {
  return String(seconds.toInt()/60);
}

inline void setWlanLED(boolean newState) {
  digitalWrite(WLAN_PIN, newState);
}

String formatIPAsString(IPAddress ip) {
  return String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
}

/*
 *  This stores WLAN credentials in the first slot [0]
 */
void storeWLANsInEEPROM(String qsid, String qpass, int idx=0, bool wlanSsid=false, bool wlanPsk=false) {
  if (wlanSsid) { // true if we will write SSID
    debugMsgln("Writing eeprom slot "+String(idx)+" SSID " + qsid,3);
    if (idx < 0 || idx > 3) return;
    wlanRead = false;                   // now needs to be re-read
    for (int i = 0; i < 32; i++) {
      if (i < qsid.length()) {
        EEPROM.write(eeWLANSSID + (32*idx) + i, qsid[i]);
        debugMsgln("Wrote: " + String(qsid[i]),5);
      } else {
        EEPROM.write(eeWLANSSID + (32*idx) + i, 0);
      }
    }
    esid[idx] = qsid; // update running config
  }
  if (wlanPsk) { // true if we will write psk
    debugMsg("Writing eeprom slot "+String(idx)+" psk ",3);
    debugMsg(qpass,9);
    debugMsgln("",3);
    for (int i = 0; i < 32; i++) {
      if ( i < qpass.length()) {
        EEPROM.write(eeWLANPASS + (32*idx) + i, qpass[i]);
        debugMsgln("Wrote: " + String(qpass[i]),9);
      } else {
        EEPROM.write(eeWLANPASS + (32*idx) + i, 0);
      }
    }
    epass[idx]=qpass; // update running config
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
    debugMsgln("Read SSID " + String(j) +":" + esid[j],3);
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
    debugMsg("Read password:",9);
    debugMsg(epass[j],9); // only show password debug level 9+
    debugMsgln("",3);
  }
}

int storeStringInEEPROM(String str, int address, int maxlen) {
  debugMsgln("writing string to EEPROM address " + String(address),3);
  for (int i = 0; i < maxlen; i++) {
    if (i < str.length()) {
      EEPROM.write(i+address, str[i]);
    } else {
      EEPROM.write(i+address, 0);
    }
  }
  debugMsg("Wrote EEPROM string, addr: " +String(address),3);
  debugMsg(" str:" + str,5);
  debugMsgln("",3);
  EEPROM.commit();
}

String getStringFromEEPROM(int address, int maxlen){ // note: std::string
  String str = "";
  byte readByte;
  for (int i = 0; i < maxlen; i++) {
    readByte = EEPROM.read(i+address);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte > 126)) { // no special chars
      continue;
    }
    str += char(readByte);
  }
  debugMsg("Read EEPROM string, addr: " +String(address),3);
  debugMsg(" str:" + str,5);
  debugMsgln("",3);
  return str;
}

void storeModelInEEPROM(String model) {
  debugMsgln("writing eeprom model " + model +" ("+String(model.length())+")",3);
  for (int i = 0; i < 16; i++)
  {
    if (i < model.length()) {
      EEPROM.write(i+eeModel, model[i]);
      debugMsgln("Wrote: " + String(model[i]),5);
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
  debugMsgln("EEPROM read model: " + model,3);
  return model;
}

void storeNtpServerInEEPROM(String server) {
  debugMsgln("eeprom writing NtpServer:" + server + " ("+String(server.length())+")",4);
  for (int i = 0; i < 32; i++)
  {
    if (i < server.length()) {
      EEPROM.write(i+eeNtpServer, server[i]);
      debugMsgln("Wrote:" + String(server[i]),5);
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
  debugMsgln("eeprom read NtpServer: " + server,4);
  return server;
}

void storeNtpTZInEEPROM(String tz) {
  debugMsgln("eeprom writing NtpTZ:" + tz + " ("+String(tz.length())+")",3);
  for (int i = 0; i < 64; i++)
  {
    if (i < tz.length()) {
      EEPROM.write(i+eeNtpTZ, tz[i]);
      debugMsgln("Wrote:" + String(tz[i]),5);
    } else {
      EEPROM.write(i+eeNtpTZ, 0);
    }
  }
  EEPROM.commit();
}

String getNtpTZFromEEPROM() {
  String tz = "";
  byte readByte;
  for (int i = 0; i < 64; i++)
  {
    readByte = EEPROM.read(i+eeNtpTZ);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte > 126)) {
      continue;
    }
    tz += char(readByte);
  }
  debugMsgln("eeprom read NtpTZ:" + tz,4);
  return tz;
}

void storeNtpPollInEEPROM(unsigned short int poll) {
  debugMsgln("eeprom writing NtpPoll:" + String(poll),4);
  EEPROM.write(eeNtpPoll, (poll >> 8));
  EEPROM.write(eeNtpPoll+1, (poll & 0xff));
  debugMsgln("Wrote:" + String(poll),5);
  EEPROM.commit();
}

unsigned short int getNtpPollFromEEPROM() {
  unsigned short int poll = 0;
  poll = EEPROM.read(eeNtpPoll) << 8;
  poll += EEPROM.read(1 + eeNtpPoll);
  debugMsgln("eeprom read NtpPoll: " + String(poll),4);
  return poll;
}

void wipeEEPROM() {
  debugMsgln(F("Wiping EEPROM."),1);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void resetEEPROM() {
  wipeEEPROM();
  debugMsgln(F("Resetting EEPROM."),1);
  for (int i = 0; i<=3 ; i++) {
    storeWLANsInEEPROM("", "", i, true, true);
  }
  storeModelInEEPROM(F("PS-PWM"));
  storeNtpServerInEEPROM(F(NTP_DEFAULT_SERVER));
  storeNtpPollInEEPROM(NTP_DEFAULT_INTERVAL);
  storeNtpTZInEEPROM(F(NTP_DEFAULT_TZ));
  std::string ap_SSID(AP_SSID);
#ifdef AP_SSID_UNIQ
  ap_SSID.append("-");
  ap_SSID.append(my_MAC.c_str());
#endif
  storeStringInEEPROM(ap_SSID.c_str(), eeAPSSID, 32);
  storeStringInEEPROM(F(AP_PSK), eeAPPSK, 32);
  storeStringInEEPROM(F(WEB_USERNAME), eeAdminName, 16);
  storeStringInEEPROM(F(WEB_PASSWORD), eeAdminPass, 16);
  storeStringInEEPROM(F(UPDATE_USERNAME), eeUpgradeName, 16);
  storeStringInEEPROM(F(UPDATE_PASSWORD), eeUpgradePass, 16);
  storeStringInEEPROM(F(JSON_PASSWORD), eeJsonPass, 16);
  storeStringInEEPROM(my_name, eeHostName, 32);

  String chkstr = F(EEPROM_SIG);
  for (int i = 0; i<=3 ; i++) {
    EEPROM.write(i + EEPROM_SIZE -4, chkstr[i]);
    debugMsgln("Wrote chkstr: " + chkstr[i],5);
  }
  EEPROM.commit();
} 

String checkEEPROM() {  
/* 
 *  Check if valid, reset if not
 */
  debugMsg(F("Check ESP EEPROM..."),1);
  String chkstr = "";
  for (int i = EEPROM_SIZE - 4; i < EEPROM_SIZE; i++) {
    chkstr += char(EEPROM.read(i));
  }
  if (chkstr != F(EEPROM_SIG)) {
    debugMsgln(F("invalid, resetting"),1);
    resetEEPROM();      
  } else {
  debugMsgln(F("valid"),1);
  }
  return chkstr;
}

void getEeConfig(){
  checkEEPROM();
  ap_SSID = getStringFromEEPROM(eeAPSSID,32).c_str();
  ap_password = getStringFromEEPROM(eeAPPSK,32).c_str();
  web_username = getStringFromEEPROM(eeAdminName,16).c_str();
  web_password = getStringFromEEPROM(eeAdminPass,16).c_str();
  root_username = getStringFromEEPROM(eeUpgradeName,16).c_str();
  root_password = getStringFromEEPROM(eeUpgradePass,16).c_str();
  json_password = getStringFromEEPROM(eeJsonPass,16).c_str();
  my_hostname = getStringFromEEPROM(eeHostName,32).c_str();
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
  debugMsgln(F("Doing ESP.restart()"),1);
  ESP.restart();         // if not, this should do a soft reboot.
}

#include <rom/rtc.h>
String get_reset_reason(int cpu) {
  RESET_REASON reason;
  String reasonString;
  reason = rtc_get_reset_reason(cpu);
  switch (reason)  {
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

String getJsButton(String buttonText, String onClick); // fwd declaration

String promptReset() {
  String response_message = "";
  bool controllerNeedsReset(); // fwd declaration
  if (controllerNeedsReset()) {
    debugMsgln(F("promptReset: reset needed"),4);
    response_message = F("<div class=\"controller\">");
    response_message += F("<br><h3>Settings changed, controller needs restart.</h3>");
    String cmd = F("restart_ctl('");
    cmd += json_password.c_str();
    cmd += "')";
    response_message += getJsButton(F("Restart Controller"), cmd );
    response_message += F("</div>");
    }
  return response_message;
}

/**
 * I2C_ClearBus
 * (http://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html)
 * (c)2014 Forward Computing and Control Pty. Ltd.
 * NSW Australia, www.forward.com.au
 * This code may be freely used for both private and commerical use
 */

/**
 * This routine turns off the I2C bus and clears it
 * on return SCA and SCL pins are tri-state inputs.
 * You need to call Wire.begin() after this to re-enable I2C
 * This routine does NOT use the Wire library at all.
 *
 * returns 0 if bus cleared
 *         1 if SCL held low.
 *         2 if SDA held low by slave clock stretch for > 2sec
 *         3 if SDA held low after 20 clocks.
 */
int I2C_ClearBus() {
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(SDA_PIN, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(SCL_PIN, INPUT_PULLUP);

  delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.

  boolean SCL_LOW = (digitalRead(SCL_PIN) == LOW); // Check is SCL is Low.
  if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(SDA_PIN) == LOW);  // vi. Check SDA input.
  int clockCount = 20; // > 2x9 clock

  while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
    clockCount--;
  // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
    pinMode(SCL_PIN, INPUT); // release SCL pullup so that when made output it will be LOW
    pinMode(SCL_PIN, OUTPUT); // then clock SCL Low
    delayMicroseconds(10); //  for >5uS
    pinMode(SCL_PIN, INPUT); // release SCL LOW
    pinMode(SCL_PIN, INPUT_PULLUP); // turn on pullup resistors again
    // do not force high as slave may be holding it low for clock stretching.
    delayMicroseconds(10); //  for >5uS
    // The >5uS is so that even the slowest I2C devices are handled.
    SCL_LOW = (digitalRead(SCL_PIN) == LOW); // Check if SCL is Low.
    int counter = 20;
    while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
      counter--;
      delay(100);
      SCL_LOW = (digitalRead(SCL_PIN) == LOW);
    }
    if (SCL_LOW) { // still low after 2 sec error
      return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
    }
    SDA_LOW = (digitalRead(SDA_PIN) == LOW); //   and check SDA input again and loop
  }
  if (SDA_LOW) { // still low
    return 3; // I2C bus error. Could not clear. SDA data line held low
  }

  // else pull SDA line low for Start or Repeated Start
  pinMode(SDA_PIN, INPUT); // remove pullup.
  pinMode(SDA_PIN, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
  // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
  /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
  delayMicroseconds(10); // wait >5uS
  pinMode(SDA_PIN, INPUT); // remove output low
  pinMode(SDA_PIN, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
  delayMicroseconds(10); // x. wait >5uS
  pinMode(SDA_PIN, INPUT); // and reset pins as tri-state inputs which is the default state on reset
  pinMode(SCL_PIN, INPUT);
  return 0; // all ok
}

//format bytes
String formatBytes(uint64_t bytes) {
  if (bytes < 1024) {
    uint32_t smallbytes = bytes;
    return String(smallbytes) + " B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + " KiB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + " MiB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GiB";
  }
}
