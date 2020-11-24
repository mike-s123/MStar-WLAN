
void setupComms() {
  #ifdef DEBUG_ON
    setupDebug();
  #endif
  pinMode(RX_PIN, INPUT);
  delay(10);
  mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);
//    remaps UART to appropriate pins for MODBUS
//    pinMatrixOutAttach(TX_PIN, U1TXD_OUT_IDX, true, false);
//    pinMatrixInAttach(RX_PIN, U1RXD_IN_IDX, true);

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
  pinMode(WLAN_PIN_OLD, OUTPUT);  // LED
}

void setupWLAN() {
  /*
 * WiFi setup
 */
  ap_ssid = ap_SSID.c_str();
  //ap_password = AP_PSK;
  WiFi.persistent(true);
  connectToWLAN();    // try to connect
  lastWLANtry = millis();
}

void setupModbus() {
  debugMsgln(F("MODBUS Starting"),1);
  node.begin(mbAddr, mbSerial);
  // Callbacks allow us to do half duplex without receiving our own transmissions
  // needed for Morningstar 6P6C connections, which are half-duplex
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  preTransmission();
  postTransmission(); // initialize
  debugMsgln("mbSerial.available="+String(mbSerial.available(),DEC),4);
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
    noController = false;
    debugMsgln(model,1);
    noController = false;
  }
}

void checkSDCard(int cardNum = 0) {
  int csPin = SD_CARD0_CS; 
  if (cardNum == 1 ) csPin = SD_CARD1_CS;    
  sd_card_available = SD.begin(csPin);
  if(!sd_card_available){
    debugMsgln("SD card mount failed",1);
  } else { // SD card mounted
    switch (SD.cardType()) { 
      case CARD_NONE:   debugMsgln(F("No SD card attached"),1);
                        break;
      case CARD_MMC:    debugMsgln(F("MMC card available"),1);
                        break;
      case CARD_SD:     debugMsgln(F("SD card available"),1);
                        break;
      case CARD_SDHC:   debugMsgln(F("SDHC card available"),1);
                        break;
      default:          debugMsgln(F("SD card unknown"),1);
                        break;
    }
    logFile = SD.open(logFileName,FILE_APPEND); // opens debug log
    debugMsgln("Opened debug log: "+logFileName,1);
    debugMsg(F("Debug on, level "),1);
    debugMsgln(String(debug_level),1);
    refreshCtlLogFile(); // opens controller log, 
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
      const char *sv = SOFTWARE_VERSION;
      String svs = sv;  // butt ugly
      debugMsgln("Version " + svs + " (" + UTC.dateTime(compileTime(), "Y-m-d~TH:i:s") + ")", 1);
      #ifdef EZT_DEBUG
        if (ezt_logFile) ezt_logFile.println(F("***********eztime.log opened***********"));
      #endif
    }
  }
}

void IRAM_ATTR SDCardIRQ(){
  sd_card_changed = true;
}

void attachSDCardIRQ(){
  pinMode(SD_DETECT, INPUT_PULLUP);  // only for SDCard0
  attachInterrupt(digitalPinToInterrupt(SD_DETECT), SDCardIRQ, CHANGE); // gets IRQ when card inserted or removed
}

void changeSDCard(){
  delay(100); // debounce
  if (digitalRead(SD_DETECT)) { // sd card removed
    debugMsgln(F("SD card removed"),1);
    debugMsgln("Closing controller log: "+ctlLogFileName,1);
    ctl_logFile.close(); // release hooks and buffers
    debugMsgln("Closing debug log: "+logFileName,1);
    logFile.close();
    SD.end();
    sd_card_available = false;
    
  } else {                // sd card inserted
    delay(100);
    if (!digitalRead(SD_DETECT)) {
      debugMsgln(F("SD card inserted"),1);
      SD.end(); // fix issue with pointers?
      checkSDCard(0);
    }
  }
  sd_card_changed = false; // reset for next time
}

void setupFS() {
  debugMsg(FS_NAME,1);
  debugMsgln(F(" starting"),1);
  if ( FILESYSTEM.begin() ) {
    debugMsg(FS_NAME,1);
    debugMsgln(F(" opened"),1);
  } else {
    debugMsg(FS_NAME,1);
    debugMsgln(F(" failed to open"),1);
  }
  checkSDCard(SD_CARD_TO_USE);
}

static void IRAM_ATTR ledIRQ(void *dummy){
  // sets a flag when LED ramping is complete.
  led_change_done = true;
}

void blinkySetup(int pin1=33, int pin2=2) {
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

  ledc_channel[1].gpio_num = pin2;
  ledc_channel[1].speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel[1].channel = LEDC_CHANNEL_1;
  ledc_channel[1].intr_type = LEDC_INTR_DISABLE;
  ledc_channel[1].timer_sel = LEDC_TIMER_0;
  ledc_channel[1].duty = (0x00000001 << (LEDC_timerbits) ) - 1;
  ledc_channel[1].hpoint = 0;

  // Set LED Controller with previously prepared configuration
  for (int ch = 0; ch < 2; ch++) {
    ledc_channel_config(&ledc_channel[ch]);
  };

  // Initialize fade service.
  ledc_fade_func_install(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED);
  ledc_isr_register(ledIRQ, &ledc_channel[0], ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED, NULL);
}
