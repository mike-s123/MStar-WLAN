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
        if (needLogTime) logFile.print(myTZ.dateTime(ATOM)+" "+String(level)+"-");
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
          logFile.print(myTZ.dateTime(ATOM)+" "+String(level)+"-");
          needLogTime = false;
        }
        logFile.print(msg);
        if (msg.endsWith("\n")) needLogTime = true;
      }
    } // level
  #endif
}

int redirectToSD(const char *szFormat, va_list args) {
#ifdef DEBUG_ON
//write evaluated format string into buffer
    int ret = vsnprintf(log_print_buff, sizeof(log_print_buff), szFormat, args);

    //output in buffer. write to file.
    if (ret >= 0 && sd_card_available && logFile){
        printf(log_print_buff);
//        logFile.write((uint8_t *)log_print_buff, (size_t)ret);
        debugMsg(log_print_buff);
    }
    return ret;
#endif    
}

void setupDebug() {
  #ifdef DEBUG_ON
    Serial.begin(BAUD_LOGGER);
    #ifdef EZT_DEBUG
      setDebug(EZT_DEBUG,ezt_logFile);
    #endif
    esp_log_set_vprintf(&redirectToSD);
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
  digitalWrite(WLAN_PIN_OLD, newState);
}

String formatIPAsString(IPAddress ip) {
  return String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
}

void putWLANs(String qsid, String qpass, int idx=0, bool wlanSsid=false, bool wlanPsk=false) {
  const char * ssidkey[4] = {"SSID1","SSID2","SSID3","SSID4"};
  const char * pskkey[4] = {"SSIDpsk1","SSIDpsk2","SSIDpsk3","SSIDpsk4"};
  preferences.begin("MStar-WLAN", false);
  if (wlanSsid) { // true if we will write SSID
    debugMsgln("pref put SSID slot "+String(idx)+" SSID " + qsid,3);
    if (idx < 0 || idx > 3) return;
    wlanRead = false;                   // now needs to be re-read
    preferences.putString(ssidkey[idx],qsid);
    esid[idx] = qsid; // update running config
  }
  if (wlanPsk) { // true if we will write psk
    debugMsg("pref put PSK slot "+String(idx)+" psk ",3);
    debugMsg(qpass,9);
    debugMsgln("",3);
    preferences.putString(pskkey[idx],qpass);
    epass[idx]=qpass; // update running config
  }
  preferences.end();
}

void getWLANs() {
  if (wlanRead) return;
  const char * ssidkey[4] = {"SSID1","SSID2","SSID3","SSID4"};
  const char * pskkey[4] = {"SSIDpsk1","SSIDpsk2","SSIDpsk3","SSIDpsk4"};
  preferences.begin("MStar-WLAN", true);
  for (int j = 0; j<=3; j++){
    esid[j] = preferences.getString(ssidkey[j],"");
    debugMsgln("Read SSID " + String(j) +":" + esid[j],3);
    epass[j] = preferences.getString(pskkey[j],"");
    debugMsg("Read password:",9);
    debugMsg(epass[j],9); // only show password debug level 9+
    debugMsgln("",3);
  }
  preferences.end();
  wlanRead=true;
  wlanSet=false;
}

void putModelPref(String model) {
  debugMsgln("pref put model " + model +" ("+String(model.length())+")",3);
  preferences.begin("MStar-WLAN", false);
  server = preferences.putString("model",model);
  preferences.end();
}

String getModelPref() {
  String model = "";
  preferences.begin("MStar-WLAN", true);
  model = preferences.getString("model",F("PS-PWM"));
  preferences.end();
  debugMsgln("pref read model: " + model,3);
  return model;
}

void putNtpServer(String server) {
  debugMsgln("pref put NtpServer:" + server + " ("+String(server.length())+")",4);
  preferences.begin("MStar-WLAN", false);
  server = preferences.putString("NtpServer",server);
  preferences.end();
}

String getNtpServer() {
  String server = "";
  preferences.begin("MStar-WLAN", true);
  server = preferences.getString("NtpServer",F(NTP_DEFAULT_SERVER));
  preferences.end();
  debugMsgln("pref read NtpServer: " + server,4);
  return server;
}

void putNtpTZ(String tz) {
  debugMsgln("pref writing NtpTZ:" + tz + " ("+String(tz.length())+")",3);
  preferences.begin("MStar-WLAN", false);
  preferences.putString("NtpTZ",tz);
  preferences.end();
}

String getNtpTZ() {
  String tz = "";
  preferences.begin("MStar-WLAN", true);
  tz = preferences.getString("NtpTZ",F(NTP_DEFAULT_TZ));
  preferences.end();
  debugMsgln("pref read NtpTZ:" + tz,4);
  return tz;
}

void putNtpPoll(unsigned short int poll) {
  debugMsgln("pref writing NtpPoll:" + String(poll),4);
  preferences.begin("MStar-WLAN", false);
  preferences.putUShort("NtpPoll",poll);
  preferences.end();
}

unsigned short int getNtpPoll() {
  unsigned short int poll;
  preferences.begin("MStar-WLAN", true);
  poll = preferences.getUShort("NtpPoll",NTP_DEFAULT_INTERVAL);
  preferences.end();
  debugMsgln("pref read NtpPoll: " + String(poll),4);
  return poll;
}

void resetPreferences(){
  preferences.begin("MStar-WLAN", false);
  preferences.clear();
  preferences.end();
}

void getPreferences(){
  preferences.begin("MStar-WLAN", true);
  ap_SSID = AP_SSID ;
  #ifdef AP_SSID_UNIQ
    ap_SSID += "-";
    ap_SSID += my_MAC;
  #endif
  ap_SSID = preferences.getString("ap_SSID", ap_SSID);
  ap_password = preferences.getString("ap_password", F(AP_PSK));
  web_username = preferences.getString("web_username", F(WEB_USERNAME));
  web_password = preferences.getString("web_password", F(WEB_PASSWORD));
  root_username = preferences.getString("root_username", F(ROOT_USERNAME));
  root_password = preferences.getString("root_password", F(ROOT_PASSWORD));
  json_password = preferences.getString("json_password", F(JSON_PASSWORD));
  my_hostname = preferences.getString("my_hostname", my_name);
  preferences.end();
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
  if (logFile) {
    logFile.flush();
    logFile.close();
  }
  delay(50);
  ESP.restart();         // this should do a soft reboot.
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
    return String(bytes / 1024.0, 3) + " KiB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0, 3) + " MiB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GiB";
  }
}

static void IRAM_ATTR ledIRQ(void *dummy){
  // sets a flag when LED ramping is complete.
  led_change_done = true;
}

void blinkySetup(int pin1, int pin2=101) {
  /*
   * call to setup blinky
   * Prepare and set configuration of timers
   * that will be used by LED Controller
   * 
   * All the ramping happens in hardware, so we setup an interrupt to signal when a ramp
   * is done.
   */
  ledc_timer_config_t ledc_timer = {LEDC_HIGH_SPEED_MODE, LEDC_timerbits, LEDC_TIMER_0, 256};
  ledc_timer_config(&ledc_timer); // Set configuration of timer0 for high speed channels
  ledc_channel[0].gpio_num = pin1;
  ledc_channel[0].speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel[0].channel = LEDC_CHANNEL_0;
  ledc_channel[0].intr_type = LEDC_INTR_FADE_END;
  ledc_channel[0].timer_sel = LEDC_TIMER_0;
  ledc_channel[0].duty = (0x00000001 << (LEDC_timerbits) ) - 1;
  ledc_channel[0].hpoint = 0;
  if (pin2 < 100) {
    ledc_channel[1].gpio_num = pin2;
    ledc_channel[1].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel[1].channel = LEDC_CHANNEL_1;
    ledc_channel[1].intr_type = LEDC_INTR_DISABLE;
    ledc_channel[1].timer_sel = LEDC_TIMER_0;
    ledc_channel[1].duty = (0x00000001 << (LEDC_timerbits) ) - 1;
    ledc_channel[1].hpoint = 0;
  }
  // Set LED Controller with previously prepared configuration
  ledc_channel_config(&ledc_channel[0]);
  if (pin2 < 100) {
    ledc_channel_config(&ledc_channel[1]);
  }

  // Initialize fade service.
  ledc_fade_func_install(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED);
  ledc_isr_register(ledIRQ, &ledc_channel[0], ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED, NULL);
}

void blinky(unsigned long int blinktime=0, unsigned long int repeattime=0, uint16_t bright=256, uint16_t dim=256) { // times in ms
  /*
   * blinky() - a very low overhead LED blinker, most stuff is handled in hardware.
   * 
   * blinktime is total time for a blink, divided between rising and falling brightness
   * call with parameters to start or change pattern, blinky(1,ULONG_MAX,0,0) to turn off fully, blinky(1,ULONG_MAX,255,255) for on.
   * call without parameters in loop() to maintain blinking. Ramping is handled in hardware, and we setup
   * an interrupt to signal when complete, so we only need to do something here if changing parameters, or
   * a ramp is complete (so we can ramp in the other direction, or pause...).
   * 
   */
  uint32_t maxduty = (( 0x00000001 << (LEDC_timerbits)) - 1);
  static uint32_t my_bright = maxduty;
  static uint32_t my_dim = 0; 
  static bool ramping_up = true;
  static uint32_t duty;
  static uint16_t ramptime, cycletime=1000;
  static uint64_t nextCycleTime = millis()+cycletime; // when the next cycle will start
//  if ( (!blinktime && !repeattime && !led_change_done) && (( millis() > nextCycleTime ) && !ramping_up) ) {
  if ( (!blinktime && !repeattime && !led_change_done) ) {
    return; // no change requested, and we're not done ramping, and waiting for end of cycle
  } else {
    if (blinktime) ramptime = blinktime/2;
    if (repeattime) cycletime = repeattime;
    if (bright<256){
      my_bright = ((255-bright)/255.0) * maxduty;
    } else {
      if (blinktime) my_bright = 0;
    }
    if (dim<256) {
      my_dim    = ((255-dim)/255.0)    * maxduty;
    } else {
      if (blinktime) my_dim = maxduty;
    }
    
    if (!ramping_up) {                                                    // we're at full dim
      if ( millis() < nextCycleTime ) {                                   // waiting for this cycle to end
        return;
      } else {  
        nextCycleTime = millis()+cycletime;                               // start of a new cycle
        ramping_up = true;
        duty = pow((float)my_bright / (float)maxduty, 0.3) * maxduty ;
        led_change_done = false;
        for (int ch = 0; ch < 2; ch++) {
          ledc_set_fade_time_and_start(ledc_channel[ch].speed_mode,
              ledc_channel[ch].channel, duty , ramptime, LEDC_FADE_NO_WAIT);
        }
      }
    } else {                  // we're at full dim
      ramping_up = false;
      duty = pow((float)my_dim / (float)maxduty,0.3) * maxduty;
      led_change_done =  false;
      for (int ch = 0; ch < 2; ch++) {
        ledc_set_fade_time_and_start(ledc_channel[ch].speed_mode,
            ledc_channel[ch].channel, duty , ramptime, LEDC_FADE_NO_WAIT);
      }
    }
  }
}

#ifdef WROVER
  uint16_t checkUSBpower(){  // USB Type-C spec 2.0, table 4.36
    if (!digitalRead(USB_PWR_DET)) return 0; // not getting power from USB
    int vR;
    int CC1 = analogReadMilliVolts(USB_CC1_PIN);
    int CC2 = analogReadMilliVolts(USB_CC2_PIN);
    (CC1 > CC2) ? vR = CC1 : vR = CC2;
    if (vR < 200) return 100;     // vRa
    if (vR < 660) return 500;     // vRd-USB
    if (vR < 1230) return 1500;   // vRd-1.5
    return 3000;                  // vRd-3.0
  }
#endif
