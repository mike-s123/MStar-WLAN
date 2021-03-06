
void setupComms() {
  #ifdef DEBUG_ON
    setupDebug();
  #endif
  pinMode(RX_PIN, INPUT);
  delay(10);
  mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);
  gpio_set_drive_capability(TX_PIN, GPIO_DRIVE_CAP_3);
  
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
  gpio_set_drive_capability((gpio_num_t)RX_ENABLE_PIN, GPIO_DRIVE_CAP_3); // maximum drive strength
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
    model = getModelPref();
    if (model == "") {
      model = "PS-MPPT";
    }
    debugMsg(F("noController, using: "),1);
    debugMsgln(model,1);
  } else {
    debugMsg(F("Got model from mbus: "),1);
    noController = false;
    debugMsgln(model,1);
    noController = false;
  }
}


void checkSDCard(int csPin = SD_CARD_CS) {
  sd_card_available = SD.begin(csPin);
  if(!sd_card_available){
    debugMsgln("SD card mount failed",1);
  } else { // SD card mounted
    
    if (!vfs_fat_registered) {
      debugMsg("sd_vfs_path: ",2);
      debugMsgln(sd_vfs_path.c_str(),2);
      esp_err_t ret = esp_vfs_fat_register(sd_vfs_path.c_str(),"",16,&FATfs);
      if (ret == ESP_OK) {
        vfs_fat_registered = true;
        debugMsgln("esp_vfs_fat_register successful");
      } else {
        debugMsgln("esp_vfs_fat_register failed");
      }
    }
        
    refreshCtlLogFile();
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
    logFile = SD.open(logFileName,FILE_APPEND); // opens platform log
    debugMsgln("Opened platform log: "+logFileName,1);
    preferences.begin(PREF_REALM, true);
    debug_level = preferences.getUInt("debug_level",DEBUG_ON);
    preferences.end();
    debugMsg(F("Debug on, level "),1);
    debugMsgln(String(debug_level),1);
//    refreshCtlLogFile(); // opens controller log, 
    #ifdef EZT_DEBUG
      ezt_logFile = SD.open(EZT_LOGFILE,FILE_APPEND);
    #endif
    if (logFile && millis() < 15000) { // just booted, not a running SD card insertion      
      debugMsgln("",1);
      debugMsgln(F("***********BOOTED***********"),1);
      debugMsg(F("Reset reason CPU 0: "),1);
      debugMsgln(get_reset_reason(0),1);
      debugMsg(F("Reset reason CPU 1: "),1);
      debugMsgln(get_reset_reason(1),1);
      const char *sv = SOFTWARE_VERSION;
      String svs = sv;  // butt ugly
      debugMsgln("Version " + svs + " (" + UTC.dateTime(compileTime(), "Y-m-d~TH:i:s") + ")", 1);
      debugMsg("Serial number: ", 1);
      debugMsgln(String(serialNumber), 1);
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
    debugMsgln("Closing platform log: "+logFileName,1);
    logFile.close();
    if (vfs_fat_registered) {
      esp_vfs_fat_unregister_path(sd_vfs_path.c_str());
      vfs_fat_registered = false;
      debugMsgln("esp_vfs_fat_unregistered");
    }
    SD.end();
    sd_card_available = false;
    
  } else {                // sd card inserted
    delay(100);
    if (!digitalRead(SD_DETECT)) {
      debugMsgln(F("SD card inserted"),1);
      SD.end(); // fix issue with pointers?
      checkSDCard(SD_CARD_CS);
    }
  }
  sd_card_changed = false; // reset for next time
}

SFFS_Volume_SPI framFS;   // SPI FRAM FileSystem instance
SFFS_File framFile(framFS); // The file instance we will use

void setupFS() {
  checkSDCard(SD_CARD_CS);
  if ( FILESYSTEM.begin() ) {  // for flash, not SD
    debugMsg(FS_NAME,1);
    debugMsgln(F(" opened"),1);
  } else {
    debugMsg(FS_NAME,1);
    debugMsgln(F(" failed to open"),1);
  }
  digitalWrite(SD_CARD_CS, HIGH);
//  if (framFS.begin(FRAM_CS,3)) {  // 3 byte address  WON'T CO-EXIST WITH SD CARD!
//    FRAMavail = true;
//  } 
}
