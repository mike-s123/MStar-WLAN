
#include <DS3231.h>   // Northern Widget LLC, et al 1.0.2, via IDE, public domain
#include <extEEPROM.h> // Jack Christensen, 3.4.1, via IDE licensed under CC BY-SA 4.0.
#include <EwmaT.h> // 1.0.2 via IDE, jonnieZG, https://github.com/jonnieZG/EWMA, MIT license
#include <ezTime.h> // 0.8.3 Rop Gonggrijp, via IDE

//#define RTCtestNtpSet 2  // deliberately sets RTC wrong for testing. 1=slow 997 ms, 2=fast 997 ms

// forward declarations
void debugMsg(String msg, int level=1);
void debugMsgln(String msg, int level=1);
void tryWLAN();
void ctlLog();
void stopLogDaily();
signed char getAgingTrim();
void oncePerFive();
void oncePerHour();
// end forward declarations

String getNtpTZ();
String getNtpServer();
unsigned short int getNtpPoll();

#define RTC_EEPROM_SIZE 4096
#define RTC_EEPROM_SIG "mjs!"

/*
 * EEPROM on DS3231 module
 * 0    (1) Last set aging trim (so we have it if battery goes away)
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
#define RTC_DRIFT_FACTOR 1.0      // multiply drift trim correction by this

#define NTP_DEFAULT_TZ "EST+5EDT,M3.2.0,M11.1.0"   // full POSIX format
#define NTP_DEFAULT_INTERVAL 7207                 // seconds, best if not a multiple of 60
#define NTP_DEFAULT_SERVER "0.pool.ntp.org"
#define NTP_MIN_INTERVAL 601                      // seconds
#define NTP_MAX_INTERVAL 65535                    // ~18 hours

DS3231 Clock;
extEEPROM rtc_eeprom(kbits_32, 1, 32, 0x57);
#define RTC_I2C_ADDR 0x68
#define RTC_EEPROM_I2C_ADDR 0x57  // ZS-042/HW-84 modules have pullups on A0-A2
#define RTC_DEFAULT_TRIM 0        // aging trim for ds3231, higher is slower
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
  debugMsgln(String(address)+": "+String(data[0]),5);
  return data[0];
}

void write_rtc_eeprom(int address, byte data) {
  debugMsg(F("Writing RTC eeprom, "),4);
  debugMsgln(String(address)+": "+String(data),4);
  byte writebyte[1];
  writebyte[0] = data;
  byte i2cStat = rtc_eeprom.write(address, writebyte, 1);
  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  delay(10);
}

void setRtcLastSetTime(time_t set_time) {
  if (!rtcPresent) return;
  debugMsgln(F("setRtcLastSetTime"),2);
  time_t unow = set_time;
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
    
time_t getRtcLastSetTime() {
  if (!rtcPresent) return 0;
  time_t utime = 0;
  for (int i=3; i>-1; i--) {  // read high byte first so we can bit shift
    utime = utime << 8;
    utime += read_rtc_eeprom(RtcEepromLastSetTime+i);
  }
  return utime;
}

time_t getUnixTime() {
  time_t unow;
  if (rtcPresent){
    RTClib RTC;
    DateTime now = RTC.now();
    unow = now.unixtime();
  } else {
    unow = UTC.now();  // from ntp
  }
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
    RTCTime += String(Hour, DEC)+": "+String(Minute, DEC)+": "+String(Second, DEC);
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
  signed char age = getAgingTrim();
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
    setRtcLastSetTime(UTC.now());
    writeRtcEepromSig();
  }  
}

bool setRtcTimeNTP() {
  if (!WiFi.isConnected()) return false;       // not unless connected
  updateNTP();
  waitForSync(3);
  if (timeStatus() == timeSet) {
    debugMsgln(F("Setting RTC from NTP"),1);
    #if RTCtestNtpSet == 1               // set 997 ms slow
      while ( ms() != 997) yield();       // for testing, deliberately set rtc wrong
    #elif RTCtestNtpSet == 2             // set 997 ms fast
      while ( ms() != 3) yield();         // for testing, deliberately set rtc wrong
      Clock.setSecond(UTC.second()+1);       // for testing, note: may wrap with minutes
    #else
      while ( ms() < 6 ) yield();        // this waits for a seconds rollover
      while ( ms() > 5 ) yield();        // then updates the RTC as fast as possible
      Clock.setSecond(UTC.second());
    #endif
    Clock.setMinute(UTC.minute());
    Clock.setHour(UTC.hour());
    Clock.setDoW(UTC.weekday());
    Clock.setDate(UTC.day());
    Clock.setMonth(UTC.month());
    Clock.setYear(UTC.year() % 100);
    setRtcLastSetTime(UTC.now());
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

signed char getAgingTrim() {
  signed char age = 0;
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(RTC_I2C_ADDR,1);
  while (Wire.available()) { age = Wire.read(); }
  return age;
}

void setAgingTrim(signed char age)  // ~0.1 ppm per, higher is slower 11.6 ppm is ~ 1 sec/day
{  
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(0x10);
    Wire.write(age);
    Wire.endTransmission();
    write_rtc_eeprom(RtcEepromAge, age);
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
  setRtcLastSetTime(UTC.now());
  setAgingTrim(RTC_DEFAULT_TRIM);
  writeRtcEepromSig();
}  

String checkRtcEeprom() {  
/* 
 *  Check if valid, reset if not
 */
  debugMsg(F("RTC EEPROM check..."),1);
  String chkstr1 = "";
  String chkstr2 = "x"; // to enter while()
  int tries = 0;
  while ( (chkstr1 != chkstr2) && tries <3 ) {
    chkstr1 = "";
    chkstr2 = "";
    for (int cnt = RTC_EEPROM_SIZE - 4; cnt < RTC_EEPROM_SIZE; cnt++) {
      chkstr1 += char(read_rtc_eeprom(cnt)); 
      chkstr2 += char(read_rtc_eeprom(cnt)); 
    }
    tries++;
  }
  if (chkstr1 != F(RTC_EEPROM_SIG)) {
    debugMsgln(F("invalid, resetting"),1);
    debugMsgln("1: "+chkstr1+", 2: "+chkstr2,1);
    resetRtcEeprom();      
  } else {
  debugMsg(chkstr1+"...",3);
  debugMsgln(F("valid"),1);
  }
  return chkstr1;
}

float getRtcppm() {
  float howlong; 
  howlong = UTC.now() - getRtcLastSetTime();          // number of seconds since we last set RTC time
  return -(rtc_diff_ewmat.output()-1000000.0) * 1000.0 / howlong;   // drift in ppm
}

void checkClocks(int32_t rtc_diff_filtered) {  
  debugMsgln(F("checkClocks"),3);
  
  if (rtcPresent && !rtcNeedsTime && (WiFi.getStatusBits() & (STA_HAS_IP_BIT | STA_HAS_IP6_BIT)) ) {
    if (timeStatus() != timeSet) { // ntp went away
        // TODO retry NTP
        debugMsgln(F("Setting time from RTC"),3);
        UTC.setTime(getUnixTime());    // from RTC to eztime
    } else {
      uint32_t howlong;
      float drift;
      int new_trim; 
      if (abs(rtc_diff_filtered) >= rtc_max_unsync) {     // only adjust if we're off by so much 
        debugMsgln(F("RTC/NTP out of sync"),3);
        if ((now() - lastNtpUpdateTime() > 120) ) {          // first make sure local drift isn't the issue
          debugMsgln(F("Poll NTP"),3);
          updateNTP(); 
          return;                                         // we'll be back in a minute if it's still off
        }
        howlong = UTC.now() - getRtcLastSetTime();            // number of seconds since we last set RTC time
        drift = -rtc_diff_filtered * 10000.0 / howlong;   // drift in 0.1 ppm, close to the DS3231 offset precision, - means RTC is slow
        new_trim = getAgingTrim() + (drift * RTC_DRIFT_FACTOR);
        debugMsg(F("howlong: "),4);
        debugMsgln(String(howlong),4);
        debugMsg(F("rtc_diff_filtered: Got model preference"),4);
        debugMsgln(String(rtc_diff_filtered),4);
        debugMsgln(F(" ppm"),4);         
        if (new_trim != getAgingTrim()) {
          if (new_trim < -127) new_trim = -127;
          if (new_trim > 127) new_trim = 127;
          if (abs(new_trim) != 127) {  // if we're at the extreme, probably an error, skip
            debugMsg(F("Changing RTC trim, drift: "),1);
            debugMsg(String(rtc_diff_filtered),1);
            debugMsg(F(" ms, ppm: "),1);
            debugMsg(String(drift/10.0,3),1); 
            debugMsg(", old trim: " + String(getAgingTrim()),1);
            debugMsgln(", new trim: " + String(new_trim),1);
            setAgingTrim(new_trim);
          } 
        } else {
          debugMsgln("RTC drift of " + String(drift/10.0,3) + "ppm within limit",2);
        }
        setRtcTimeNTP(); //resync
      }
    }
  }
}

/*
 * IRQ, RTC marks millis() every second
 */
static void IRAM_ATTR rtcIRQ() { // handles interrupts from RTC, can't do much here
  rtc_ms = millis();  // tracks difference between ntp and rtc
  rtc_IRQ = true;
  if (--myWDT <= 1) ESP.restart(); // loop watchdog
  ;
}

/*
 * Time diff processing happens once per minute, using a timestamp of a 1/sec
 * RTC interrupt.
 * 
 * The result is real_rtc_diff_ms, which is where an RTC second is positioned
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
  int32_t ms_diff;
  int32_t real_rtc_diff_ms;
  
  deleteEvent(oncePerMinute); 
  setEvent(oncePerMinute,now()+60);  // for next time

  debugMsgln(F("oncePerMinute"),3);  
  if ( (timeStatus() == timeSet) && rtcPresent && !rtcNeedsTime ) { // ntp and rtc running  
    uint32_t opm_millis = millis();  // grab some times immediately
    uint16_t opm_ms = UTC.ms();
    uint32_t opm_secs = UTC.now();
    rtc_IRQ = false;           // watch for IRQ while getting time
    uint32_t opm_rtc_ms = rtc_ms;
    rtc_secs = getUnixTime();  // what second does rtc think it is?
    if (rtc_IRQ) { // got an IRQ, timestamps may be wrong, try again
      debugMsgln(F("RTC IRQ while processing, try again"),4);
      opm_millis = millis();
      opm_ms = UTC.ms();
      opm_secs = UTC.now();
      rtc_IRQ = false;
      opm_rtc_ms = rtc_ms;
      rtc_secs = getUnixTime();
      if (rtc_IRQ) { // didn't expect that, we just got one...
        debugMsgln(F("Second RTC IRQ while processing time, aborting"),1);
        return;
      }
    }
    ms_diff = -(opm_millis - opm_ms - opm_rtc_ms);
    debugMsgln("opm_millis: "+String(opm_millis),5);
    debugMsgln("opm_ms: "+String(opm_ms),5);
    debugMsgln("opm_rtc_ms: "+String(opm_rtc_ms),5);
    debugMsgln("ms_diff: "+String(ms_diff),5);
  
    if (ms_diff < -999 || ms_diff > 999) { // sanity check, 
      debugMsg(F("ms_diff out of bounds: "),2);
      debugMsgln(String(ms_diff),3);
      return;
    }
    real_rtc_diff_ms = ((opm_secs-rtc_secs) * 1000) + ms_diff;
    debugMsgln("rtc_secs: "+String(rtc_secs),5);
    debugMsgln("opm_secs: "+String(opm_secs),5);
    debugMsgln("ms_diff: "+String(ms_diff),5);
    debugMsgln("real_rtc_diff_ms: "+String(real_rtc_diff_ms),5);
    // moving average-like filter, 1000000 (1000 second) offset because it underflows on negative numbers
    rtc_diff_filtered = rtc_diff_ewmat.filter(real_rtc_diff_ms+1000000)-1000000; 
    debugMsgln("RTC diff real/filt: " + String(real_rtc_diff_ms) + "/" + String(rtc_diff_filtered),4);
    
    if (timeStatus() == timeSet && abs(real_rtc_diff_ms - rtc_diff_filtered) < 5 ) {  // only if NTP is running and
      checkClocks(rtc_diff_filtered);                                                 // we've averaged to within 5 ms
    }                                                                                                    
  }
  // other oncePerMinute processing goes here
  ctlLog(); // log controller data
}

void oncePerFive() { // every 5 minutes
  deleteEvent(oncePerFive);
  UTC.setEvent(oncePerFive,UTC.now()+300);
  debugMsgln(F("oncePerFive"),3);
  tryWLAN(); // try to connect as station
  if (logFile) logFile.flush();      // flush logs every 5 minutes
  if (ctl_logFile) ctl_logFile.flush();
  #ifdef EZT_DEBUG
    if (ezt_logFile) ezt_logFile.flush();
  #endif
}

void oncePerHour() { // not necessarily _on_ the hour
  deleteEvent(oncePerHour); 
  UTC.setEvent(oncePerHour,UTC.now()+3600);
  debugMsgln(F("oncePerHour"),3);
  if ((timeStatus() == timeNeedsSync) && rtcPresent) UTC.setTime(getUnixTime());     // lost ntp sync, update from RTC
  timeval epoch = {UTC.now(), UTC.ms()}; // FAT is not TZ aware, use local TZ
  settimeofday((const timeval*)&epoch, 0); // set ESP ToD, for SD file timestamps
  debugMsgln(F("settimeofday"),4);
  if (myTZ.hour() % 6 == 0 && !noController) { // every 6 hours
      debugMsgln("Modbus errors/tries: " + String(mbuserrs) + "/" + String(mbustries) + " (" + String(((double)mbuserrs/(double)mbustries)*100.,3) + "%)", 1);
  }
  // TODO logrotate?
}

void midnight() {
  deleteEvent(midnight); 
  myTZ.setEvent(midnight,myTZ.now()+86400);
  debugMsgln("Midnight",1);
  stopLogDaily(); // save todays info
}
void eventConfirm() {
  debugMsgln(F("Events are running"),1);
}

void setupClocks() {
  unsigned short int ntp_temp;
  bool rtcNeedsTrim = false;

  // first, check for RTC
  if (!Wire.requestFrom(RTC_I2C_ADDR, 2)) {
    debugMsg(F("No "),1);
  } else {
   rtcPresent = true;
  }
  debugMsgln(F("RTC found"),1);

  if (rtcPresent) { // found RTC
    byte i2cStat = rtc_eeprom.begin(rtc_eeprom.twiClock100kHz); // check RTC EEPROM
    if ( i2cStat != 0 ) {
      debugMsgln(F("I2C Problem with RTC eeprom"),1);
    }
    delay(10);
    if (!Clock.oscillatorCheck()) {  // check for Oscillator Stopped Flag (!good RTC)
      debugMsgln(F("RTC OSC stopped"),1);
      rtcNeedsTrim = true; // probably need to reload offset and set time, too
      rtcNeedsTime = true;
      setRtc(false); // set dummy time, clear OSF
    }
  }
  if (rtcPresent) {  //previous check might have failed, found RTC, and it passed checks
    checkRtcEeprom();
    if (rtcNeedsTrim) setAgingTrim(getAgingTrim());
    debugMsgln(F("RTC configuring interrupts"),1);
    Clock.enableOscillator(true, false, 0);                            // Ena SQW output, but not on batt, 1 Hz
    attachInterrupt(digitalPinToInterrupt(SQW_PIN), rtcIRQ, FALLING);  // runs rtcIRQ once a second, falling edge (start of second)    
  } // using rtc

    
  // now, setup NTP
  myTZ.setDefault();                     // setup NTP service
  tzPosix = getNtpTZ();
  debugMsg(F("Setting NtpTZ to: "),1);
  debugMsgln(tzPosix,1);
  ntpTZ = tzPosix;
  myTZ.setPosix(tzPosix);
  ntpServer = getNtpServer();
  debugMsg(F("Setting NtpServer to: "),1);
  debugMsgln(ntpServer,1);
  setServer(ntpServer);
  tzPosix = ""; // we were just using it temporarily
  ntp_temp = getNtpPoll();
  debugMsg(F("Setting NtpPoll to: "),1);
  debugMsgln(String(ntp_temp),1);
  setInterval(ntp_temp);
  ntpInterval = ntp_temp;
  rtc_max_unsync = RTC_MAX_UNSYNC * sqrt(ntpInterval/600);
  delay(100);    
  debugMsgln(F("NTP trying to sync..."),1);
  waitForSync(15);
  ntp_state = timeStatus();
  if (ntp_state == timeSet) {
    debugMsg(F("NTP time is "),1);
    debugMsgln(myTZ.dateTime(RFC850),1);
    timeval epoch = {UTC.now(), UTC.ms()}; // FAT is not TZ aware, use local TZ
    settimeofday((const timeval*)&epoch, 0); // set ESP ToD, for SD file timestamps
    if (rtcNeedsTime) rtcNeedsTime = setRtcTimeNTP(); 
  } else {
    debugMsgln(F("NTP not sync'd"),1);
    const char* remote_host = ntpServer.c_str();
  /*  if(Ping.ping(remote_host)) {
      debugMsg("Can ping ",1);
    } else {
      debugMsg("Can't ping ",1);
    }  
    debugMsgln(ntpServer,1);
   */ 
    if (rtcPresent && !rtcNeedsTime ) {
      debugMsgln(F("Setting time from RTC"),1);
      UTC.setTime(getUnixTime()); // from RTC to eztime
      debugMsg(F("RTC time is "),1);
      debugMsgln(myTZ.dateTime(RFC850),1);
    }
    ntp_state = timeStatus();  // tell NTP to keep trying
  }
  bootTime = UTC.now();      // record when we booted, setupClocks only called once during setup()
}

time_t getMidnight() { // returns time of local midnight in UTC
  tmElements_t tm ;
  breakTime(myTZ.now(), tm);
  tm.Hour = 0; tm.Minute = 0; tm.Second = 0; // beginning of today
  time_t midnight = makeTime(tm) + (myTZ.getOffset() * 60) + 86400;
  debugMsgln("midnight is "+String(midnight),3);
  return(midnight);  
}

void setEvents() { // set timed events even if not ntp sync'd
  debugMsgln(F("Creating timed events"),1);
  deleteEvent(oncePerMinute);
  deleteEvent(oncePerFive);
  deleteEvent(oncePerHour);
  deleteEvent(eventConfirm);
  deleteEvent(midnight);
  UTC.setEvent(oncePerMinute,( UTC.now()-UTC.now()%60 ) +60 ); // top of minute (0 second)
  UTC.setEvent(oncePerFive,UTC.now()+20);
  UTC.setEvent(oncePerHour,UTC.now()+10);
  UTC.setEvent(eventConfirm,UTC.now()+5);
  UTC.setEvent(midnight, getMidnight());
}

void checkNtp() {  // watches for changes to ntp state
  if ( ntp_state != timeStatus() ) { //state changed
    ntp_state = timeStatus();
    debugMsg(F("NTP state changed, "),1);
    debugMsgln(ntp_state_string(ntp_state),1);
    if ( (ntp_state == timeSet) && rtcNeedsTime ) {
      setRtcTimeNTP();
      setEvents();
    }
  }
}
