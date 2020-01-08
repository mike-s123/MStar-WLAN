
#include <DS3231.h>   // Northern Widget LLC, et al 1.0.2, via IDE, public domain
#include <extEEPROM.h> // Jack Christensen, 3.4.1, via IDE licensed under CC BY-SA 4.0.
#include <EwmaT.h> // 1.0.2 via IDE, jonnieZG, https://github.com/jonnieZG/EWMA, MIT license
#include <ezTime.h> //Rop Gonggrijp 1.8.3, via IDE

//#define RTCtestNtpSet 2  // deliberately sets RTC wrong for testing. 1=slow 997 ms, 2=fast 997 ms

void debugMsg(String msg, int level);
void debugMsgln(String msg, int level);
void tryWLAN();

signed char getAgingOffset(); // fwd declaration
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
#define RTC_DRIFT_FACTOR 0.9      // multiply drift offset correction by this
/* TESTING
 * esp8266 (75565f) -1.73 ppm drift, changed offset from 0 to -22 (factor 1.3) on 12/3, 11:26
 *  0.69 drift (~40% overcorrected), -22 to -15 12/5 13:56
 * 
 * esp32 (d42fa9) 1.14 drift, rtc 0, changed offset from -32 to -21 12/3, 20:16, 0.9 factor
 *  0.571 drift, -21 to -14
 * 
 * esp32 (d42fa9) drift, rtc 5, 
 * esp32 (d42fa9) drift, rtc -6, 
 */

#define NTP_DEFAULT_TZ "EST5EDT,M3.2.0,M11.1.0"   //POSIX format
#define NTP_DEFAULT_INTERVAL 7207                 // seconds, best if not a multiple of 60
#define NTP_DEFAULT_SERVER "0.pool.ntp.org"
#define NTP_MIN_INTERVAL 601                      // seconds
#define NTP_MAX_INTERVAL 65535                    // ~18 hours

DS3231 Clock;
extEEPROM rtc_eeprom(kbits_32, 1, 32, 0x57);
#define RTC_I2C_ADDR 0x68
#define RTC_EEPROM_I2C_ADDR 0x57  // ZS-042/HW-84 modules have pullups on A0-A2
#define RTC_DEFAULT_OFFSET 0      // aging offset for ds3231, higher is slower
                                  // ~0.1 ppm per (~9 ms/day, 0.26 sec/month), higher is slower, 
                                  // 11.6 ppm is ~ 1 sec/day
#define RTC_MAX_UNSYNC 125        // adjust when we're this many ms out of sync with NTP, for 601 sec NTP polls
#define SQW_PIN 14                // GPIO 14 (D5) on both, SQW from DS3231

bool rtcPresent = false;
bool rtcNeedsTime = false;
timeStatus_t ntp_state;           // timeNotSet, timeSet, timeNeedsSync
bool twentyFourHourClock = true;
bool Century=false;
bool h12;
bool PM;
int Year = 2019;
byte Month = 4, Day = 1, Weekday = 1, Hour = 0, Minute = 0, Second = 0;  // April fool's
volatile long int rtc_ms = 0;
// below underflows with negatives, so all uses add 1000000.
EwmaT <long int> rtc_diff_ewmat(2, 10, 1000000); // 2/10:weight for new additions, 1000000:starting value
long int rtc_diff_filtered = 0; // filtered value
int rtc_IRQ = false;  // faster to set an int than a bool?
unsigned long int rtc_secs;
String ntpServer = NTP_DEFAULT_SERVER;
unsigned short int ntpInterval = NTP_MAX_INTERVAL;
String ntpTZ = NTP_DEFAULT_TZ;  //posix string
String tzName;  // used for lookup
String tzPosix; // used for lookup
Timezone myTZ;
unsigned int rtc_max_unsync = RTC_MAX_UNSYNC * sqrt(ntpInterval/600);  // can get more out of sync with longer ntp updates


String ntp_state_string(timeStatus_t state) {
  if ( state == timeSet ) return F("NTP time set");
  if ( state == timeNotSet ) return F("NTP time not set");
  if ( state == timeNeedsSync ) return F("NTP time needs sync");
  return F("err"); 
}
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
  debugMsg(F("Reading RTC eeprom, "),5);
  debugMsgln(String(address)+":"+String(data[0]),5);
  return data[0];
}

void write_rtc_eeprom(int address, byte data) {
  debugMsg(F("Writing RTC eeprom, "),4);
  debugMsgln(String(address)+":"+String(data),4);
  byte writebyte[1];
  writebyte[0] = data;
  byte i2cStat = rtc_eeprom.write(address, writebyte, 1);
  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  delay(10);
}

void setRtcLastSetTime(uint32_t set_time) {
  if (!rtcPresent) return;
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
  if (!rtcPresent) return 0;
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
  debugMsgln("Wrote RTC eeprom sig: " + chkstr,3);
}

// ************************************************************
// Get the time from the RTC
// ************************************************************
void getRtcTime() {
  if (rtcPresent) {
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

String getRtcTimeString(int full = 1) {  // form: 1=full, 0=HMS
  if (rtcPresent) {
    String RTCTime = "";
    getRtcTime();
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


void setRtc(boolean writeee=false) {
  Clock.setClockMode(false); // false = 24h
  Clock.setYear(Year % 100);
  Clock.setMonth(Month);
  Clock.setDate(Day);
  Clock.setDoW(Weekday);
  Clock.setHour(Hour);
  Clock.setMinute(Minute);
  Clock.setSecond(Second);  // The OSF is cleared by function setSecond();.
  signed char age = getAgingOffset();
  rtc_diff_ewmat.reset(); //reset averaging filter
  rtc_diff_filtered = 0;
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
    debugMsgln(F("Setting RTC from NTP"),1);
    #if RTCtestNtpSet == 1               // set 997 ms slow
      while ( ms() != 997) yield();       // for testing, deliberately set rtc wrong
    #elif RTCtestNtpSet == 2             // set 997 ms fast
      while ( ms() != 3) yield();         // for testing, deliberately set rtc wrong
      Clock.setSecond(second()+1);       // for testing, note: may wrap with minutes
    #else
      while ( ms() < 6 ) yield();        // this waits for a seconds rollover
      while ( ms() > 5 ) yield();        // then updates the RTC as fast as possible
      Clock.setSecond(second());
    #endif
    Clock.setMinute(minute());
    Clock.setHour(hour());
    Clock.setDoW(weekday());
    Clock.setDate(day());
    Clock.setMonth(month());
    Clock.setYear(year() % 100);
    setRtcLastSetTime(now());
    rtc_diff_ewmat.reset(); //reset averaging filter
    rtc_diff_filtered = 0;
    return true;
  } else {
    return false;
  }
}

bool setRtcTime(String rtcTime) {
  debugMsg(F("setRtcTime to "),1);
  debugMsgln(rtcTime,1);
  Year = rtcTime.substring(0,4).toInt();
  Month = rtcTime.substring(4,6).toInt();
  Day = rtcTime.substring(6,8).toInt();
  Weekday = rtcTime.substring(8,9).toInt();
  Hour = rtcTime.substring(9,11).toInt();
  Minute = rtcTime.substring(11,13).toInt();
  Second = rtcTime.substring(13).toInt();
  setRtc(true);
}

// ************************************************************
// Get the temperature from the RTC
// ************************************************************
float getRtcTemp() {
  if (rtcPresent) {
    return Clock.getTemperature();
  } else {
    return 0.0;
  }
}

signed char getAgingOffset() {
  signed char age = 0;
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(RTC_I2C_ADDR,1);
  while (Wire.available()) { age = Wire.read(); }
  return age;
}

void setAgingOffset(signed char offset)  // ~0.1 ppm per, higher is slower 11.6 ppm is ~ 1 sec/day
{  
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(0x10);
    Wire.write(offset);
    Wire.endTransmission();
    write_rtc_eeprom(RtcEepromAge, offset);
    rtc_diff_ewmat.reset(); //reset averaging filter
    rtc_diff_filtered = 0;
    /*
     * New values, including changes to the AGE register, are loaded only when a change in the temperature 
     * value occurs, or when a user-initiated temperature conversion is completed.
     */
    getRtcTemp();
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
  debugMsg(F("RTC EEPROM check..."),1);
  String chkstr = "";
  for (int i = RTC_EEPROM_SIZE - 4; i < RTC_EEPROM_SIZE; i++) {
    chkstr += char(read_rtc_eeprom(i)); 
  }
  if (chkstr != F(RTC_EEPROM_SIG)) {
    debugMsgln(F("invalid, resetting"),1);
    resetRtcEeprom();      
  } else {
  debugMsgln(F("valid"),1);
  }
  return chkstr;
}

float getRtcppm() {
  float howlong; 
  howlong = now() - getRtcLastSetTime();          // number of seconds since we last set RTC time
  return -(rtc_diff_ewmat.output()-1000000.0) * 1000.0 / howlong;   // drift in ppm
}

void checkClocks(long int rtc_diff_filtered) {  
  debugMsgln(F("checkClocks"),3);
  if (rtcPresent && !rtcNeedsTime) {
    if (timeStatus() != timeSet) { // ntp went away
        // TODO retry NTP
        setTime(getUnixTime());    // from RTC to eztime
    } else {
      unsigned long int howlong;
      float drift;
      int new_offset; 
      if (abs(rtc_diff_filtered) >= rtc_max_unsync) {     // only adjust if we're off by so much 
        if (now() - lastNtpUpdateTime() > 120) {          // first make sure local drift isn't the issue
          updateNTP(); 
          return;                                         // we'll be back in a minute if it's still off
        }
        howlong = now() - getRtcLastSetTime();            // number of seconds since we last set RTC time
        drift = -rtc_diff_filtered * 10000.0 / howlong;   // drift in 0.1 ppm, close to the DS3231 offset precision, - means RTC is slow
        new_offset = getAgingOffset() + (drift * RTC_DRIFT_FACTOR);
        debugMsg(F("howlong:"),4);
        debugMsgln(String(howlong),4);
        debugMsg(F("rtc_diff_filtered:"),4);
        debugMsgln(String(rtc_diff_filtered),4);
        debugMsgln(F(" ppm"),4);         
        if (new_offset != getAgingOffset()) {
          if (new_offset < -127) new_offset = -127;
          if (new_offset > 127) new_offset = 127;
          if (abs(new_offset) != 127) {  // if we're at the extreme, probably an error, skip
            debugMsgln(F("Changing RTC offset"),1);
            debugMsg(F("Drift:"),1);
            debugMsgln(String(drift/10.0),1); 
            debugMsgln("Old offset:" + String(getAgingOffset()),1);
            debugMsgln("New offset:" + String(new_offset),1);
            setAgingOffset(new_offset);
          }
        }
        setRtcTimeNTP(); //resync
      }
    }
  }
}

/*
 * IRQ, RTC marks millis() every second
 */
void ICACHE_RAM_ATTR rtcIRQ() { // handles interrupts from RTC, can't do much here
  rtc_ms = millis();  // tracks difference between ntp and rtc
  rtc_IRQ = true;
  ;
}

/*
 * Time diff processing happens at the beginning of each ntp second.
 * result is real_rtc_diff_ms, which is where an RTC second is positioned
 * against an ntp second. A negative value indicates it occured before the
 * ntp second.
 * 
 * We start by collecting timestamps from NTP and RTC. If an RTC interrupt
 * occurs while getting time from it, it's invalid so we try again. That should
 * always work, because the next IRQ shouldn't be until a second later.
 * 
 * rtc_ms is when the RTC sent an interrupt at the start of its last second, 
 * in millis(). If oncePerMinute gets delayed before running, that will be
 * reflected in opm_ms. So, we subtract that from current millis() to adjust. 
 * So, the last rtc second happened at:
 * 
 *   ms_diff = -(opm_millis - opm_ms - rtc_ms);
 * 
 * If ms_diff is decreasing, RTC is running faster than NTP (getting more ahead). 
 * If exactly sync'd rtc_secs = opm_secs and ms_diff = 0.
 * 
 * Then, we have to look at whole seconds. If opm_secs>rtc_secs, the rtc has gotten
 * behind (late) by more a second, so include that. 
 *  
 *   real_rtc_diff_ms = ((opm_secs-rtc_secs) * 1000) + ms_diff;
 * 
 * Note that the local time can change by ms per minute, because
 * it depends on the ESP crystal/resonator, which may not be particularly 
 * accurate. So, the difference may rarely converge exactly. So, we allow a 
 * few ms difference when adjusting.
 * 
 */
void oncePerMinute() { // not necessarily _on_ the minute
  long int ms_diff;
  long int real_rtc_diff_ms;
  setEvent(oncePerMinute,now()+60);
  debugMsgln(F("oncePerMinute"),2);
  
  if ( (timeStatus() == timeSet) && rtcPresent && !rtcNeedsTime ) { // ntp and rtc running  
    long int opm_millis = millis();  // grab some times immediately
    int opm_ms = ms();
    long int opm_secs = now();
    rtc_IRQ = false;           // watch for IRQ while getting time
    long int opm_rtc_ms = rtc_ms;
    rtc_secs = getUnixTime();  // what second does rtc think it is?
    if (rtc_IRQ) { // got an IRQ, timestamps may be wrong, try again
      debugMsgln(F("RTC IRQ while processing, try again"),4);
      opm_millis = millis();
      opm_ms = ms();
      opm_secs = now();
      rtc_IRQ = false;
      opm_rtc_ms = rtc_ms;
      rtc_secs = getUnixTime();
    }
    if (rtc_IRQ) { // didn't expect that, we just got one...
      debugMsgln(F("Second RTC IRQ while processing time, aborting"),1);
      return;
    }
    ms_diff = -(opm_millis - opm_ms - opm_rtc_ms);
    debugMsgln("opm_millis:"+String(opm_millis),4);
    debugMsgln("opm_ms:"+String(opm_ms),4);
    debugMsgln("opm_rtc_ms:"+String(opm_rtc_ms),4);
    debugMsgln("ms_diff:"+String(ms_diff),4);
  
    if (ms_diff < -999 || ms_diff > 999) { // sanity check, 
      debugMsg(F("ms_diff out of bounds:"),3);
      debugMsgln(String(ms_diff),3);
      return;
    }
    real_rtc_diff_ms = ((opm_secs-rtc_secs) * 1000) + ms_diff;
    debugMsgln("rtc_secs:"+String(rtc_secs),5);
    debugMsgln("opm_secs:"+String(opm_secs),5);
    debugMsgln("ms_diff:"+String(ms_diff),5);
    debugMsgln("real_rtc_diff_ms:"+String(real_rtc_diff_ms),5);
    // moving average-like filter, 1000000 (1000 second) offset because it underflows on negative numbers
    rtc_diff_filtered = rtc_diff_ewmat.filter(real_rtc_diff_ms+1000000)-1000000; 
    debugMsgln("RTC diff real/filt:" + String(real_rtc_diff_ms) + "/" + String(rtc_diff_filtered),3);
    if (timeStatus() == timeSet && abs(real_rtc_diff_ms - rtc_diff_filtered) < 5 ) {  // only if NTP is running and
      checkClocks(rtc_diff_filtered);                                                 // we've averaged to within 5 ms
    }                                                                                                    
  }
  // other oncePerMinute processing goes here
}

void oncePerFive() { // every 5 minutes
  setEvent(oncePerFive,now()+300);
  debugMsgln(F("oncePerFive"),2);
  tryWLAN(); // try to connect as station
}

void oncePerHour() { // not necessarily _on_ the hour
  setEvent(oncePerHour,now()+3600);
  debugMsgln(F("oncePerHour"),2);
  // TODO logrotate
  if ((timeStatus() == timeNeedsSync) && rtcPresent) setTime(getUnixTime());     // lost ntp sync, update from RTC
}

void setupClocks() {
  unsigned short int ntp_temp;
  bool rtcNeedsOffset = false;

  // first, check for RTC
  if (!Wire.requestFrom(RTC_I2C_ADDR, 2)) {
    debugMsg(F("No "),1);
  } else {
   rtcPresent = true;
  }
  debugMsgln(F("RTC found"),1);
  byte i2cStat = rtc_eeprom.begin(rtc_eeprom.twiClock100kHz);
  if ( i2cStat != 0 ) {
    debugMsgln(F("I2C Problem with RTC eeprom"),1);
  }

  if (rtcPresent) { // found RTC
    delay(10);
    if (!Clock.oscillatorCheck()) {  // check for Oscillator Stopped Flag (!good RTC)
      debugMsgln(F("RTC OSC stopped"),1);
      rtcNeedsOffset = true; // probably need to reload offset and set time, too
      rtcNeedsTime = true;
      setRtc(false); // set dummy time, clear OSF
    }
  }
  if (rtcPresent) {  //previous check might have failed, found RTC, and it passed checks
    checkRtcEeprom();
    if (rtcNeedsOffset) setAgingOffset(getAgingOffset());
    debugMsgln(F("RTC configuring interrupts"),1);
    Clock.enableOscillator(true, false, 0);                            // Ena SQW output, but not on batt, 1 Hz
    attachInterrupt(digitalPinToInterrupt(SQW_PIN), rtcIRQ, FALLING);  // runs rtcIRQ once a second, falling edge (start of second)
  } // using rtc

    
  // now, setup NTP
  myTZ.setDefault();                     // setup NTP service
  tzPosix = getNtpTZFromEEPROM();
  //  if (tzPosix != "") {
  //   tzPosix = ntpTZ);
  //  }
  debugMsg(F("Setting NtpTZ to:"),1);
  debugMsgln(tzPosix,1);
  ntpTZ = tzPosix;
  myTZ.setPosix(tzPosix);
  tzPosix = getNtpServerFromEEPROM(); // tzPosix is used as a temp
  debugMsg(F("Setting NtpServer to:"),1);
  debugMsgln(tzPosix,1);
  setServer(tzPosix);
  ntpServer = tzPosix;
  tzPosix = ""; // we were just using it temporarily
  ntp_temp = getNtpPollFromEEPROM();
  debugMsg(F("Setting NtpPoll to:"),1);
  debugMsgln(String(ntp_temp),1);
  setInterval(ntp_temp);
  ntpInterval = ntp_temp;
  rtc_max_unsync = RTC_MAX_UNSYNC * sqrt(ntpInterval/600);
  delay(100);    
  if (timeStatus() != timeSet) {      // wait a bit for sync
    debugMsgln(F("NTP Trying to sync."),1);
    waitForSync(10);
  }
  ntp_state = timeStatus();
  if (ntp_state == timeSet) {
    debugMsg(F("NTP Sync'd Time is "),1);
    debugMsgln(myTZ.dateTime(RFC850),1);
    if (rtcNeedsTime) {
      setRtcTimeNTP(); 
      rtcNeedsTime = false;
    }
  } else {
    debugMsgln(F("NTP not sync'd"),1);
    if (rtcPresent && !rtcNeedsTime ) {
      debugMsgln(F("Setting time from RTC"),1);
      myTZ.setTime(getUnixTime()); // from RTC to eztime 
    }
    
 // tell NTP to keep trying
    ntp_state = timeStatus();
  }

  debugMsgln(F("Creating timed events"),1);
  setEvent(oncePerMinute,now()+40); // set timed events even if not ntp sync'd
  setEvent(oncePerFive,now()+50);
  setEvent(oncePerHour,now()+60); 
}

void checkNtp() {  // watches for changes to ntp state
  if ( ntp_state != timeStatus() ) { //state changed
    ntp_state = timeStatus();
    debugMsg(F("NTP state changed, "),2);
    debugMsgln(ntp_state_string(ntp_state),2);
    if ( ntp_state == timeSet && rtcNeedsTime ) {
      setRtcTimeNTP();
      deleteEvent(oncePerMinute);          // reset timed events
      setEvent(oncePerMinute,now()+10); 
      deleteEvent(oncePerFive);
      setEvent(oncePerFive,now()+20);
      deleteEvent(oncePerHour);
      setEvent(oncePerHour,now()+30); 
    }
  }
}

/*
void sdDateTime(uint16_t* date, uint16_t* time) {
  // User gets date and time from GPS or real-time
  // clock in real callback function

  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(year(), month(), day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(hour(), minute(), second());
}
*/
