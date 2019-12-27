
#include <DS3231.h>   // Northern Widget LLC, et al 1.0.2, via IDE, public domain
#include <extEEPROM.h> // Jack Christensen, 3.4.1, via IDE licensed under CC BY-SA 4.0.
#include <EwmaT.h> // 1.0.2 via IDE, jonnieZG, https://github.com/jonnieZG/EWMA, MIT license
#include <ezTime.h> //Rop Gonggrijp 1.8.3, via IDE

void debugMsgContinue(String msg); // fwd declaration 
void debugMsg(String msg) ;
int getAgingOffset(); // fwd declaration
String getNtpTZFromEEPROM();
String getNtpServerFromEEPROM();
unsigned short int getNtpPollFromEEPROM();

#define RTC_EEPROM_SIZE 4096
#define RTC_EEPROM_SIG "mjs!"

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
 * 4092-4095 (4)  Valid signature (RTC_EEPROM_SIG)
 */
#define RtcEepromAge 0
#define RtcEepromYear 1
#define RtcEepromMonth 3
#define RtcEepromDay 4
#define RtcEepromHour 5
#define RtcEepromMinute 6
#define RtcEepromSecond 7
#define RtcEepromLastSetTime 8

#define NTP_DEFAULT_TZ "EST5EDT,M3.2.0,M11.1.0"   //POSIX format
#define NTP_DEFAULT_INTERVAL 7207                 // seconds, best if not a multiple of 60
#define NTP_DEFAULT_SERVER "0.pool.ntp.org"
#define NTP_MIN_INTERVAL 601                      // seconds
#define NTP_MAX_INTERVAL 64999                    // ~18 hours

DS3231 Clock;
extEEPROM rtc_eeprom(kbits_32, 1, 32, 0x57);
#define RTC_I2C_ADDR 0x68
#define RTC_EEPROM_I2C_ADDR 0x57 // ZS-042/HW-84 modules have pullups on A0-A2
#define RTC_DEFAULT_OFFSET 0       // aging offset for ds3231, higher is slower
                            // ~0.1 ppm per (~9 ms/day, 0.26 sec/month), higher is slower, 
                            // 11.6 ppm is ~ 1 sec/day
#define SQW_PIN 14        // GPIO 14 (D5) on both, SQW from DS3231

bool useRTC = false;
bool twentyFourHourClock = true;
bool Century=false;
bool h12;
bool PM;
int Year = 2019;
byte Month = 4, Day = 1, Weekday = 1, Hour = 0, Minute = 0, Second = 0;  // April fool's
volatile long int rtc_500ms = 500;
EwmaT <long int> rtc_diff_ewmat(3, 10, 500); // 3/10:weight for new additions, 0:starting value
int rtc_diff_filtered = 500; // filtered value
String ntpServer = NTP_DEFAULT_SERVER;
unsigned short int ntpInterval = NTP_MAX_INTERVAL;
String ntpTZ = NTP_DEFAULT_TZ;  //posix string
String tzName;  // used for lookup
String tzPosix; // used for lookup
Timezone myTZ;


void setupRtcSQW() {
  pinMode(SQW_PIN, INPUT);            // From DS3231 square wave output 1 Hz - 8.192 KHz
  digitalWrite(SQW_PIN, HIGH);        // internal pullup
}

// ************************************************************
// Read/write the eeprom on the RTC module
// ************************************************************
byte read_rtc_eeprom(int address) {
  // Read a byte at address in EEPROM memory.
  byte data[1];
  byte i2cStat = rtc_eeprom.read(address, data, 1);
  #if DEBUG_ON>3
    debugMsgContinue(F("Reading RTC eeprom, "));
    debugMsg(String(address)+":"+String(data[0]));
  #endif
  return data[0];
}

void write_rtc_eeprom(int address, byte data) {
//  return;
  #if DEBUG_ON>2
    debugMsgContinue(F("Writing RTC eeprom, "));
    debugMsg(String(address)+":"+String(data));
  #endif
  byte writebyte[1];
  writebyte[0] = data;
  byte i2cStat = rtc_eeprom.write(address, writebyte, 1);
  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  delay(10);
}

void setRtcLastSetTime(uint32_t set_time) {
  if (!useRTC) return;
  uint32_t unow = set_time;
  write_rtc_eeprom(RtcEepromYear, Year>>8);
  write_rtc_eeprom(RtcEepromYear+1, Year%256);
  write_rtc_eeprom(RtcEepromMonth, Month);
  write_rtc_eeprom(RtcEepromDay, Day);  
  write_rtc_eeprom(RtcEepromHour, Hour);
  write_rtc_eeprom(RtcEepromMinute, Minute);
  write_rtc_eeprom(RtcEepromSecond, Second);
  for (int i=0 ; i<4 ; i++) {
    write_rtc_eeprom(RtcEepromLastSetTime+i, unow%256);
    unow = unow >> 8;
  }
}
    
uint32_t getRtcLastSetTime() {
  if (!useRTC) return 0;
  uint32_t utime = 0;
  for (int i=3; i>-1; i--) {  // read high byte first so we can bit shift
    utime = utime << 8;
    utime += read_rtc_eeprom(RtcEepromLastSetTime+i);
  }
  return utime;
}

uint32_t getUnixTime() {
  RTClib RTC;
  DateTime now = RTC.now();
  uint32_t unow = now.unixtime();
  return unow;
}

void writeRtcEepromSig() {
  String chkstr = F(RTC_EEPROM_SIG);
  for (int i = 0; i < 4 ; i++) {
    write_rtc_eeprom(i + RTC_EEPROM_SIZE -4, chkstr[i]);
  }
  #if DEBUG_ON>2
    debugMsg("Wrote RTC eeprom sig: " + chkstr);
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


void setRTC(boolean writeee=false) {
  Clock.setClockMode(false); // false = 24h
  Clock.setYear(Year % 100);
  Clock.setMonth(Month);
  Clock.setDate(Day);
  Clock.setDoW(Weekday);
  Clock.setHour(Hour);
  Clock.setMinute(Minute);
  Clock.setSecond(Second);  // The OSF is cleared by function setSecond();.
  byte age = getAgingOffset();
  if (writeee) {
    write_rtc_eeprom(RtcEepromYear, Year>>8);
    write_rtc_eeprom(RtcEepromYear+1, Year%256);
    write_rtc_eeprom(RtcEepromMonth, Month);
    write_rtc_eeprom(RtcEepromDay, Day);  
    write_rtc_eeprom(RtcEepromHour, Hour);
    write_rtc_eeprom(RtcEepromMinute, Minute);
    write_rtc_eeprom(RtcEepromSecond, Second);
    write_rtc_eeprom(RtcEepromAge, age);
    setRtcLastSetTime(now());
    writeRtcEepromSig();
  }  
}

bool setRtcTimeNTP() {
  updateNTP();
  waitForSync(3);
  if (timeStatus() == timeSet) {
    while ( ms() < 6 ) yield();        // this waits for a seconds rollover
    while ( ms() > 5 ) yield();        // then updates the RTC as fast as possible
    Clock.setSecond(second());
    Clock.setMinute(minute());
    Clock.setHour(hour());
    Clock.setDoW(weekday());
    Clock.setDate(day());
    Clock.setMonth(month());
    Clock.setYear(year() % 100);
    setRtcLastSetTime(now());
    return true;
  } else {
    return false;
  }
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
  setRTC(true);
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
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(RTC_I2C_ADDR,1);
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
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(0x10);
    Wire.write(offset);
    Wire.endTransmission();
    write_rtc_eeprom(RtcEepromAge, offset);
    rtc_diff_ewmat.reset(); //reset averaging filter
}

void resetRtcEeprom() {
  setRtcTimeNTP();
  setRtcLastSetTime(now());
  setAgingOffset(RTC_DEFAULT_OFFSET);
  writeRtcEepromSig();
}  

String checkRtcEeprom() {  
/* 
 *  Check if valid, reset if not
 */
  #if DEBUG_ON>0
    debugMsgContinue(F("RTC EEPROM check..."));
  #endif    
  String chkstr = "";
  for (int i = RTC_EEPROM_SIZE - 4; i < RTC_EEPROM_SIZE; i++) {
    chkstr += char(read_rtc_eeprom(i)); 
  }
  if (chkstr != F(RTC_EEPROM_SIG)) {
    #if DEBUG_ON>0
      debugMsg(F("invalid, resetting"));
    #endif  
    resetRtcEeprom();      
  } else {
  #if DEBUG_ON>0
    debugMsg(F("valid"));
  #endif  
  }
  return chkstr;
}

void checkClocks(int rtc_diff_filtered) {  
  #if DEBUG_ON>0
    debugMsg(F("checkClocks"));
  #endif
  if (useRTC) {
    if (timeStatus() != timeSet) { // ntp went away
        // TODO retry NTP
        setInterval(0);            // disable ntp
        setTime(getUnixTime());    // from RTC to eztime
    } else {
      long int howlong;
      float drift;
      int new_offset, diff;
      diff = rtc_diff_filtered - 500;
      if (abs(diff) > 125) {                     // only adjust if we're off by more than 125 ms
        if (now() - lastNtpUpdateTime() > 120) { // seconds, first make sure local drift isn't the issue
          updateNTP(); 
          return;                                // we'll be back in a minute if it's still off
        }
        howlong = now() - getRtcLastSetTime();   // number of seconds since we last set
        drift = diff * 10000.0 / howlong;        // drift in 0.1 ppm, which matches the DS3231 offset precision
        new_offset = getAgingOffset() - drift;
        #if DEBUG_ON>2
          debugMsgContinue(F("rtc_diff_filtered:"));
          debugMsg(String(rtc_diff_filtered));
          debugMsgContinue(F("diff:"));
          debugMsg(String(diff));
          debugMsgContinue(F("drift:"));
          debugMsg(String(drift));          
        #endif
        if (new_offset != getAgingOffset()) {
          if (new_offset < -127) new_offset = -127;
          if (new_offset > 127) new_offset = 127;
          if (abs(new_offset) != 127) {  // if we're at the extreme, probably an error, skip
            #if DEBUG_ON>0
              debugMsg(F("Changing RTC offset"));
              debugMsg("Old offset:" + String(getAgingOffset()));
              debugMsg("New offset:" + String(new_offset));
            #endif        
            setAgingOffset(new_offset);
          }
        }
        setRtcTimeNTP(); //resync
      }
    }
  }
}

void ICACHE_RAM_ATTR rtcIRQ() { // handles interrupts from RTC, can't do much here
  rtc_500ms = millis();  // tracks difference between ntp and rtc
  ;
}

void oncePerMinute() { // not necessarily _on_ the minute
  long int real_rtc_diff, my_ms = ms(), diff;
  setEvent(oncePerMinute,now()+60);
  #if DEBUG_ON>1
    debugMsg(F("oncePerMinute"));
  #endif
  diff = millis() - ms();             // some large number
  real_rtc_diff = rtc_500ms - diff + 1000;  // find when rtc irq fired, in ms() terms, it's about a half-second before now
  if (real_rtc_diff < 0 || real_rtc_diff > 999) {
    #if DEBUG_ON>1
      debugMsgContinue(F("real_rtc_diff out of bounds:"));
      debugMsg(String(real_rtc_diff));
    #endif
    return;
  }
  //TODO make sure NTP/RTC are within a second
  rtc_diff_filtered = rtc_diff_ewmat.filter(real_rtc_diff); // moving average-like filter
  checkClocks(rtc_diff_filtered);
  #if DEBUG_ON>1
    debugMsg("RTC diff real/filt:" + String(real_rtc_diff) + "/" + String(rtc_diff_filtered));
  #endif        
}

void oncePerHour() { // not necessarily _on_ the hour
  setEvent(oncePerHour,now()+3600);
  #if DEBUG_ON>1
    debugMsg(F("oncePerHour"));
  #endif        
}

void setupClocks() {
    unsigned short int ntp_temp;
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
    delay(1000);    
    if (timeStatus() == timeNotSet) {      // try to sync with NTP a few times
      #if DEBUG_ON>0
        debugMsg(F("NTP Trying to sync."));
      #endif
      for (int i=0; (i<5) && (timeStatus() == timeNotSet); i++) {  
        waitForSync(2);
      }
    }

    if (timeStatus() == timeSet) {
      #if DEBUG_ON>0
        debugMsgContinue(F("NTP Sync'd. Time is "));
        debugMsg(myTZ.dateTime(RFC850));
        debugMsg(F("Creating timed events"));
      #endif
      delay(1000);
      setEvent(oncePerMinute,now()+10); // 
      setEvent(oncePerHour,now()+20); 
    }
  }

    if (!Wire.requestFrom(RTC_I2C_ADDR, 2)) {
      #if DEBUG_ON>0
        debugMsgContinue(F("No "));
      #endif      
    } else {
     useRTC = true;
    }
    #if DEBUG_ON>0
      debugMsg(F("RTC found"));
    #endif   
      byte i2cStat = rtc_eeprom.begin(rtc_eeprom.twiClock100kHz);
    #if DEBUG_ON>0
      if ( i2cStat != 0 ) {
        debugMsg(F("I2C Problem with RTC eeprom"));
      }
    #endif
      
   if (useRTC) {
    delay(10);
    if (!Clock.oscillatorCheck()) {  // check for Oscillator Stopped Flag (!good RTC)
      #if DEBUG_ON>0
        debugMsg(F("RTC starting"));
      #endif
      setRTC(true);   // Clears OSF flag
      delay(2);
      if (!Clock.oscillatorCheck()) {
        useRTC = false;  // didn't start up
        #if DEBUG_ON>0
          debugMsg(F("RTC check failed"));  // probably no RTC
        #endif
      }
    } 
    // found RTC, and it passed checks
    checkRtcEeprom();
  }  
  
  if (useRTC) {
    if (timeStatus() != timeSet) { // no ntp, but rtc avail
      setInterval(0);  // disable ntp
      setTime(getUnixTime()); // from RTC to eztime
    } else { // both ntp and rtc
      #if DEBUG_ON>0
        debugMsg(F("RTC configuring interrupts"));
      #endif
      Clock.enableOscillator(true, false, 0);  // Ena osc, not on batt, 1 Hz
      attachInterrupt(digitalPinToInterrupt(SQW_PIN), rtcIRQ, RISING);  // runs rtcIRQ once a second, rising edge (on half second)
    }
  }
}
