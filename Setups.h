
void setupComms() {
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
    #ifdef DEBUG_ON
      delay(100);
      setupDebug();
      delay(100);
      debugMsg(F("Debug on, level "),1);
      debugMsgln(String(debug_level),1);
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
    #ifdef DEBUG_ON
      setupDebug();
      debugMsg(F("Debug on, level "),1);
      debugMsgln(String(debug_level),1);
    #endif
    pinMode(RX_PIN, INPUT);
    delay(100);
    mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);    
//    pinMatrixOutAttach(TX_PIN, U1TXD_OUT_IDX, true, false);
//    pinMatrixInAttach(RX_PIN, U1RXD_IN_IDX, true);
  #endif

  int rtn = I2C_ClearBus(); // clear the I2C bus first before calling Wire.begin()
  if (rtn != 0) {
    debugMsgln(F("I2C bus error. Could not clear"),1);
    if (rtn == 1) {
      debugMsgln(F("SCL clock line held low"),1);
    } else if (rtn == 2) {
      debugMsgln(F("SCL clock line held low by slave clock stretch"),1);
    } else if (rtn == 3) {
      debugMsgln(F("SDA data line held low"),1);
    }
  } else { // bus clear
    debugMsgln(F("I2C bus clear, Wire.begin()"),1);
    // re-enable Wire
    // now can start Wire Arduino master
    Wire.begin(SDA_PIN, SCL_PIN);              // setup I2C
  }

  pinMode(RX_ENABLE_PIN, OUTPUT);            // used for half-duplex MODBUS
  rxEnable(false);
  pinMode(WLAN_PIN, OUTPUT);  // LED
}

void setupWLAN() {
  /*
 * WiFi setup
 */

  #ifdef AP_SSID_UNIQ
    ap_SSID.append("-");
    ap_SSID.append(my_MAC.c_str());
  #endif
  ap_ssid = ap_SSID.c_str();
  //ap_password = AP_PSK;

  WiFi.persistent(true);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.hostname(my_hostname);
  #endif
  
  #ifdef ARDUINO_ARCH_ESP32
  {
    WiFi.mode(WIFI_STA);
    char __hostname[sizeof(my_hostname)+1];
    my_hostname.toCharArray(__hostname, sizeof(__hostname));
    // __hostname[sizeof(my_hostname)] = '\0';
    //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(__hostname);             // TODO not working
  }
  #endif
  
  debugMsg(F("Using hostname: "),1);
  debugMsgln(my_hostname,1);
  debugMsgln(F("Trying to connect to WLAN."),1);
  
  wlanConnected = connectToWLAN();    // try to connect as STA
  lastWLANtry = millis();
}

void setupModbus() {
  debugMsgln(F("MODBUS Starting"),1);
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
    debugMsg(F("Got model from EEPROM:"),1);
    debugMsgln(model,1);
  } else {
    debugMsg(F("Got model from mbus:"),1);
    debugMsgln(model,1);
    noController = false;
  }
}


#ifdef ARDUINO_ARCH_ESP32

  bool checkSDCard(int cardNum = 0) {
    int csPin = SD_CARD0_CS; 
    if (cardNum == 1 ) csPin = SD_CARD1_CS;    
    sd_card_available = SD.begin(csPin);
    if(!sd_card_available){
      debugMsgln("SD card mount failed",1);
    } else { // SD card mounted
      logFile = SD.open(logFileName,FILE_APPEND);
      ctl_logFile = SD.open(CTL_LOGFILE,FILE_APPEND);
      #ifdef EZT_DEBUG
        ezt_logFile = SD.open(EZT_LOGFILE,FILE_APPEND);
      #endif
      if (logFile && millis() < 15000) { // just booted, not a running SD card insertion      
        debugMsgln("",1);
        debugMsgln(F("***********BOOTED***********"),1);
        debugMsg(F("Reset reason CPU 0:"),1);
        debugMsgln(get_reset_reason(0),1);
        debugMsg(F("Reset reason CPU 1:"),1);
        debugMsgln(get_reset_reason(1),1);
        #ifdef EZT_DEBUG
          if (ezt_logFile) ezt_logFile.println(F("***********eztime.log opened***********"));
        #endif
      }
      switch (SD.cardType()) { 
        case CARD_NONE:   debugMsgln(F("No SD card attached"),1);
                          break;
        case CARD_MMC:    debugMsgln(F("MMC card available"),1);
                          break;
        case CARD_SD:     debugMsgln(F("SD card availaable"),1);
                          break;
        case CARD_SDHC:   debugMsgln(F("SDHC card available"),1);
                          break;
        default:          debugMsgln(F("SD card unknown"),1);
                          break;
      }
    }
  }

  void IRAM_ATTR SDCardIRQ(){
    sd_card_changed = true;
  }

  void setupSDCard(){
    pinMode(SD_DETECT, INPUT_PULLUP);  // only for SDCard0
    attachInterrupt(digitalPinToInterrupt(SD_DETECT), SDCardIRQ, CHANGE); 
  }

  void changeSDCard(){
    delay(100); // debounce
    if (digitalRead(SD_DETECT)) { // sd card removed
      SD.end();
      sd_card_available = false;
      debugMsgln(F("SD card removed"),1);
    } else {                // sd card inserted
      delay(100);
      if (!digitalRead(SD_DETECT)) {
        debugMsgln(F("SD card inserted"),1);
        checkSDCard(0);
      }
    }
    sd_card_changed = false; // reset for next time
  }

#endif //esp32


/* playing with SdFat.h
#ifdef ARDUINO_ARCH_ESP32
  bool checkSDCard(int cardNum = 0) {
    int csPin = SD_CARD0_CS; 
    pinMode(SD_DETECT, INPUT_PULLUP);  // TODO future hardware, only for SDCard0
    if (cardNum == 1 ) csPin = SD_CARD1_CS;
    SdFile::dateTimeCallback(sdDateTime);
    sd_card_available = SD.begin(csPin);
    if(!sd_card_available){
      debugMsgln("SD card mount failed",1);
    } else {
      if (sd_card_log) {
//          String lfn = "/"+ my_hostname + ".log";
//          File logFile = SD.open(lfn,FILE_WRITE); // starts new log
//          if (logFile) {
//            logFile.print(myTZ.dateTime(ATOM)+" ");
//            logFile.println(F("Log cleared"));
//          }
        debugMsgln("",1);
        debugMsgln(F("***********BOOTED***********"),1);
        debugMsg(F("Reset reason CPU 0:"),1);
        debugMsgln(get_reset_reason(0),1);
        debugMsg(F("Reset reason CPU 1:"),1);
        debugMsgln(get_reset_reason(1),1);
      }
      switch (SD.cardType()) { 
        case CARD_NONE:   debugMsgln(F("No SD card attached"),1);
                          break;
        case CARD_MMC:    debugMsgln(F("MMC card attached"),1);
                          break;
        case CARD_SD:     debugMsgln(F("SD card attached"),1);
                          break;
        case CARD_SDHC:   debugMsgln(F("SDHC card attached"),1);
                          break;
        default:          debugMsgln(F("SD card unknown"),1);
                          break;
      }
    } 
  }
#endif //esp32
*/

void setupFS() {
  
  debugMsg(fs_type,1);
  debugMsgln(F(" Starting"),1);
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
    debugMsg(FS_TYPE,1);
    debugMsgln(F(" opened"),1);
  } else {
    debugMsg(FS_TYPE,1);
    debugMsgln(F(" failed to open"),1);
  }
  #ifdef ARDUINO_ARCH_ESP32
    checkSDCard(SD_CARD_TO_USE);
  #endif  //esp32
}
