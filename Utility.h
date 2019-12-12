// ----------------------------------------------------------------------------------------------------
// ----------------------------------------- Utility functions ----------------------------------------
// ----------------------------------------------------------------------------------------------------

void toggleBlueLED() {
  blueLedState = ! blueLedState;
  digitalWrite(WIFI_PIN, blueLedState);
}

void setBlueLED(boolean newState) {
  digitalWrite(WIFI_PIN, newState);
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
    cSerial->begin(9600);
  }
  
  void rxEnable(bool state); // forward declaration
  
  void serialPassthrough() {  // this connects Serial (controller) to cSerial (USB)
    static long int txdTimer;
    
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

String getSSIDFromEEPROM() {
  String esid = "";
  for (int i = 0; i < 32; i++)
  {
    byte readByte = EEPROM.read(i);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte == 0xFF)) {
      continue;
    }
    esid += char(readByte);
  }
  #if DEBUG_ON>2
  debugMsg("Read SSID: " + esid);
  #endif
  return esid;
}

String getPasswordFromEEPROM() {
  String epass = "";
  for (int i = 32; i < 96; i++)
  {
    byte readByte = EEPROM.read(i);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte == 0xFF)) {
      continue;
    }
    epass += char(EEPROM.read(i));
  }
  #if DEBUG_ON>2
    debugMsgContinue("Read password");
  #endif
  #if DEBUG_ON>5                            // only show password debug level 6+
    debugMsgContinue(": " + epass);
  #endif
  #if DEBUG_ON>2
    debugMsg("");
  #endif
  return epass;
}

String getModelFromEEPROM() {
  String model = "";
  for (int i = 96; i < 112; i++)
  {
    byte readByte = EEPROM.read(i);
    if (readByte == 0) {
      break;
    } else if ((readByte < 32) || (readByte == 0xFF)) {
      continue;
    }
    model += char(EEPROM.read(i));
  }
  #if DEBUG_ON>2
    debugMsg("Read EEPROM model: " + model);
  #endif
  return model;
}

void storeCredentialsInEEPROM(String qsid, String qpass) {
  #if DEBUG_ON>2
  debugMsg("writing eeprom ssid " + qsid );
  #endif
  for (int i = 0; i < 32; i++)
  {
    if (i < qsid.length()) {
      EEPROM.write(i, qsid[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote: " + String(qsid[i]));
      #endif
    } else {
      EEPROM.write(i, 0);
    }
  }
  #if DEBUG_ON>2
  debugMsg("writing eeprom pass " + qpass);
  #endif
  for (int i = 0; i < 96; i++)
  {
    if ( i < qpass.length()) {
      EEPROM.write(32 + i, qpass[i]);
      #if DEBUG_ON>5
      debugMsg("Wrote: " + String(qpass[i]));
      #endif
    } else {
      EEPROM.write(32 + i, 0);
    }
  }

  EEPROM.commit();
}

void storeModelInEEPROM(String model) {
  #if DEBUG_ON>2
  debugMsg("writing eeprom model " + model +" ("+String(model.length())+")");
  #endif
  for (int i = 0; i < 16; i++)
  {
    if (i < model.length()) {
      EEPROM.write(i+96, model[i]);
      #if DEBUG_ON>4
      debugMsg("Wrote: " + String(model[i]));
      #endif
    } else {
      EEPROM.write(i+96, 0);
    }
  }
  EEPROM.commit();
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
  storeCredentialsInEEPROM("", "");
  storeModelInEEPROM(F("PS-PWM"));
  String chkstr = F(EEPROM_SIG);
  for (int i = 0; i < 4 ; i++) {
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
    debugMsgContinue(F("Check EEPROM..."));
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
  WiFi.mode(WIFI_AP);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif
  #if DEBUG_ON>0
    debugMsgContinue(F("Starting AP, SSID \""));
    debugMsgContinue(String(ap_ssid));
    debugMsgContinue(F("\", pass \""));
    debugMsgContinue(String(ap_password));
    debugMsg(F("\""));
  #endif  
//  WiFi.softAPConfig(apIP, IPAddress(192, 168, 99, 254), IPAddress(255, 255, 255, 0));
  if (strlen(password) > 0) {
    WiFi.softAP(ssid, password, 6, false, 8);
  } else {
    WiFi.softAP(ssid);
  }
//  WiFi.softAPConfig(apIP, IPAddress(192, 168, 99, 254), IPAddress(255, 255, 255, 0));  // for good measure
}


boolean connectToWLAN(const char* ssid, const char* password) {
/*
 *  Try to connect as a station with the given credentials. Give up after 10 seconds or 20 retries
 *   if we can't get in. Returns connected status.
 *   Station only, because AP_STA results in frequent disconnects (~5 minute intervals).
*/
  int retries = 0;
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif
  WiFi.mode(WIFI_STA);  // station only 
  
  if (password && strlen(password) > 0 ) {    
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);
  }

  #if DEBUG_ON>0
    debugMsgContinue(F("Connecting to WLAN"));
  #endif

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    #if DEBUG_ON>0
      debugMsgContinue(".");
    #endif
    retries++;
    if (retries > 30) {
      #if DEBUG_ON>0
        debugMsg("");
      #endif      
      return false;
    }
  }
//  WiFi.mode(WIFI_AP_STA);  // trying station only
  return true;
}

String formatIPAsString(IPAddress ip) {
  return String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
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

// ************************************************************
// Read/write the eeprom on the RTC module
// ************************************************************
byte read_clk_eeprom(int address) {
  // Read a byte at address in EEPROM memory.
  byte data = clk_eeprom->readByte(address);
  #if DEBUG_ON>2
    debugMsgContinue(F("Reading clk eeprom, "));
    debugMsg(String(address)+":"+String(data));
  #endif  
  return data;
}
void write_clk_eeprom(int address, byte data) {
  clk_eeprom->writeByte(address, data);
  #if DEBUG_ON>2
    debugMsgContinue(F("Writing clk eeprom, "));
    debugMsg(String(address)+":"+String(data));
  #endif  
  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  delay(10);
}

uint32_t geteeUnixTime() {
  uint32_t utime = 0;
  for (int i=3; i>-1; i--) {  // read high byte first so we can bit shift
    utime = utime << 8;
    utime += read_clk_eeprom(eeUnix+i);
  }
  return utime;
}

uint32_t getUnixTime() {
  RTClib RTC;
  DateTime now = RTC.now();
  uint32_t unow = now.unixtime();
  return unow;
}

void write_clk_sig() {
  String chkstr = F(CLK_EEPROM_SIG);
  for (int i = 0; i < 4 ; i++) {
    write_clk_eeprom(i + CLK_EEPROM_SIZE -4, chkstr[i]);
  }
  #if DEBUG_ON>2
    debugMsg("Wrote clk_chkstr: " + chkstr);
  #endif
}

// ************************************************************
// Get the time from the RTC
// ************************************************************
void getRTCTime() {
  if (useRTC) {
    Year = Clock.getYear() + 2000;
    Month = Clock.getMonth(Century);
    Day = Clock.getDate();
    Weekday = Clock.getDoW();
    Hour = Clock.getHour(twentyFourHourClock, PM);
    Minute = Clock.getMinute();
    Second = Clock.getSecond();
    // Make sure the clock keeps running even on battery
    if (!Clock.oscillatorCheck())
      Clock.enableOscillator(true, true, 0);
  } 
}

String getRTCTimeString(int full = 1) {  // form: 1=full, 0=HMS
  if (useRTC) {
    String RTCTime = "";
    getRTCTime();
    if (full) {
      RTCTime = String(Month)+"/"+String(Day, DEC)+"/"+String(Year, DEC)+" ";
    }
    RTCTime += String(Hour, DEC)+":"+String(Minute, DEC)+":"+String(Second, DEC);
    return RTCTime;
  }
}

// ************************************************************
// Set the date/time in the RTC 
// ************************************************************
int getAgingOffset(); // fwd declaration

void setRTC() {
  Clock.setClockMode(false); // false = 24h
  Clock.setYear(Year % 100);
  write_clk_eeprom(eeYear, Year>>8);
  write_clk_eeprom(eeYear+1, Year%256);
  Clock.setMonth(Month);
  write_clk_eeprom(eeMonth, Month);
  Clock.setDate(Day);
  write_clk_eeprom(eeDay, Day);  
  Clock.setDoW(Weekday);
  Clock.setHour(Hour);
  write_clk_eeprom(eeHour, Hour);
  Clock.setMinute(Minute);
  write_clk_eeprom(eeMinute, Minute);
  Clock.setSecond(Second);
  write_clk_eeprom(eeSecond, Second);
  byte age = getAgingOffset();
  write_clk_eeprom(eeAge, age);
  uint32_t unow = getUnixTime();
  for (int i=0 ; i<4 ; i++) {
    write_clk_eeprom(eeUnix+i, unow%256);
    unow = unow >> 8;
  }
  write_clk_sig();
}

bool setRtcTime(String rtcTime) {
  #if DEBUG_ON>0
    debugMsgContinue(F("setRtcTime to "));
    debugMsg(rtcTime);
  #endif
  Year = rtcTime.substring(0,4).toInt();
  Month = rtcTime.substring(4,6).toInt();
  Day = rtcTime.substring(6,8).toInt();
  Weekday = rtcTime.substring(8,9).toInt();
  Hour = rtcTime.substring(9,11).toInt();
  Minute = rtcTime.substring(11,13).toInt();
  Second = rtcTime.substring(13).toInt();
  setRTC();
}

// ************************************************************
// Get the temperature from the RTC
// ************************************************************
float getRTCTemp() {
  if (useRTC) {
    return Clock.getTemperature();
  } else {
    return 0.0;
  }
}

int getAgingOffset() {
  int b = 0, r = 0;
  Wire.beginTransmission(DS3231_I2C);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C,1);
  while (Wire.available()) { b = Wire.read(); }
  double temp = 0;
  if (b>127) { r =  b - 256; } 
  else       { r =  b; }
  return r;
}

void setAgingOffset(int offset)  // ~0.1 ppm per, higher is slower 11.6 ppm is ~ 1 sec/day
{  
  if (offset < -127 || offset > 127) return;  // out of range
  
    if (offset < 0) offset += 256;    // set sign bit
    Wire.beginTransmission(DS3231_I2C);
    Wire.write(0x10);
    Wire.write(offset);
    Wire.endTransmission();
    write_clk_eeprom(eeAge, offset);
}
