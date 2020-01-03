
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
    #if DEBUG_ON>0
      delay(100);
      setupDebug();
      delay(100);
      debugMsgContinue(F("Debug on, level "));
      debugMsg(String(DEBUG_ON));
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
    #if DEBUG_ON>0
      setupDebug();
      debugMsgContinue(F("Debug on, level "));
      debugMsg(String(DEBUG_ON));
    #endif
    pinMode(RX_PIN, INPUT);
    delay(100);
    mbSerial.begin(9600, SERIAL_8N2, RX_PIN, TX_PIN, true);    
//    pinMatrixOutAttach(TX_PIN, U1TXD_OUT_IDX, true, false);
//    pinMatrixInAttach(RX_PIN, U1RXD_IN_IDX, true);
  #endif

  int rtn = I2C_ClearBus(); // clear the I2C bus first before calling Wire.begin()
  if (rtn != 0) {
    #if DEBUG_ON>0
      debugMsg(F("I2C bus error. Could not clear"));
      if (rtn == 1) {
        debugMsg(F("SCL clock line held low"));
      } else if (rtn == 2) {
        debugMsg(F("SCL clock line held low by slave clock stretch"));
      } else if (rtn == 3) {
        debugMsg(F("SDA data line held low"));
      }
    #endif
  } else { // bus clear
    #if DEBUG_ON>0
      debugMsg(F("I2C bus clear, Wire.begin()"));
    #endif
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

  WiFi.macAddress(mac);
  #ifdef AP_SSID_UNIQ
    ap_SSID.append("-");
    ap_SSID.append((String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX)).c_str());
  #endif
  ap_ssid = ap_SSID.c_str();
  //ap_password = AP_PSK;
  my_hostname = HOSTNAME + String("-") + String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);

WiFi.persistent(true);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.hostname(my_hostname);
  #endif
  
  #ifdef ARDUINO_ARCH_ESP32
  {
    WiFi.mode(WIFI_STA);
    char __hostname[sizeof(my_hostname)+1];
    my_hostname.toCharArray(__hostname, sizeof(__hostname));
    WiFi.setHostname(__hostname);             // TODO not working
  }
  #endif
  
  #if DEBUG_ON>0
    debugMsgContinue(F("Using hostname: "));
    debugMsg(my_hostname);
  #endif

  #if DEBUG_ON>0
    debugMsg(F("Trying to connect to WLAN."));
  #endif
  
  wlanConnected = connectToWLAN();    // try to connect as STA
  lastWLANtry = millis();
}

void setupModbus() {
    #if DEBUG_ON>0
    debugMsg(F("MODBUS Starting"));
  #endif
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
  #if DEBUG_ON>0
    debugMsgContinue(F("Got model from EEPROM:"));
    debugMsg(model);
  #endif
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(F("Got model from mbus:"));
      debugMsg(model);
    #endif
    noController = false;
  }
}

#ifdef ARDUINO_ARCH_ESP32
  bool checkSDCard() {
    pinMode(SD_DETECT, INPUT_PULLUP);  // TODO future hardware
    SD_card = SD.begin();
    #if DEBUG_ON > 0
      if(!SD_card){
        debugMsg("SD card mount failed");
      } else {
        switch (SD.cardType()) { 
          case CARD_NONE:   debugMsg(F("No SD card attached"));
                            break;
          case CARD_MMC:    debugMsg(F("MMC card attached"));
                            break;
          case CARD_SD:     debugMsg(F("SD card attached"));
                            break;
          case CARD_SDHC:   debugMsg(F("SDHC card attached"));
                            break;
          default:          debugMsg(F("SD card unknown"));
                            break;
        }
      }
    #endif //debug
  }
#endif //esp32

void setupFS() {
  #if DEBUG_ON>0
    debugMsgContinue(fs_type);
    debugMsg(F(" Starting"));
  #endif
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
    #if DEBUG_ON>0
      debugMsgContinue(FS_TYPE);
      debugMsg(F(" opened"));
    #endif
    ;
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(FS_TYPE);
      debugMsg(F(" failed to open"));
    #endif
    ;    
  }
  #ifdef ARDUINO_ARCH_ESP32
    checkSDCard();
  #endif  //esp32
}
