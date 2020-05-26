
//-------------------------------------------------------------------
//------------------------- Page handlers  --------------------------
//-------------------------------------------------------------------

void statusPageHandler (AsyncWebServerRequest *request) {
/*  
 *   Returns a page of basic controller status.
 */
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psStatusPageHandler(request);
  }
}

void setChargePageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set controller charging settings.
 */
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psSetChargePageHandler(request);
  }
}

void setOtherPageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psSetOtherPageHandler(request);
  }
}

void cmdPageHandler(AsyncWebServerRequest *request) {                          
/*
 *  Handles POST requests made to /cmd
 *  Used by JavaScript
 */
  int addr, offset, wlan, numArgs = request->args();
  unsigned short int ntp_poll = -1;
  String data, value, ssid, pass, response_message = F("OK"), rtcTime, ntp_item, ntp_svr = "", ntp_tz = "", var = "";
  enum commands { read_reg, write_reg, read_coil, write_coil, set_rtc, set_aging, set_wlan, set_rtc_ntp, cfg_ntp, clr_dlog, clr_clog };
  commands cmd;
  debugMsg(F("SET args received:"),4);
  debugMsgln(String(numArgs),4);
  for (int i=0 ; i<numArgs ; i++) {
    debugMsgln("SET arg#"+String(i)+", "+request->argName(i)+":"+request->arg(i),4);
    if ( request->argName(i) == F("writereg") ) {
      cmd = write_reg;
      addr = request->arg(i).toInt();
    } else if ( request->argName(i) == F("readreg") ) {
      cmd = read_reg;
      addr = request->arg(i).toInt();
    } else if ( request->argName(i) == F("writecoil") ) {
      cmd = write_coil;
      addr = request->arg(i).toInt();
    } else if ( request->argName(i) == F("readcoil") ) {
      cmd = read_coil;
      addr = request->arg(i).toInt();
    } else if ( request->argName(i) == F("clr_dlog") ) {
      cmd = clr_dlog;
    } else if ( request->argName(i) == F("clr_clog") ) {
      cmd = clr_clog;
    } else if ( request->argName(i) == F("setrtc") ) {
      cmd = set_rtc;
      rtcTime = request->arg(i);
    } else if ( request->argName(i) == F("setagingoffset") ) {
      cmd = set_aging;
      offset = request->arg(i).toInt();
    } else if ( request->argName(i) == F("setwlan") ) {
      cmd = set_wlan;
      wlan = request->arg(i).toInt();
    } else if ( request->argName(i) == F("data") ) {
      data = request->arg(i);
    } else if ( request->argName(i) == F("ssid") ) {
      ssid = request->arg(i);
    } else if ( request->argName(i) == F("pass") ) {
      pass = request->arg(i);
    } else if ( request->argName(i) == F("setrtcntp") ) {
      cmd = set_rtc_ntp;
    } else if ( request->argName(i) == F("setntpcfg") ) {
      debugMsgln(F("/cmd, setntpcfg received"),2);
      cmd = cfg_ntp;
      ntp_item = request->arg(i);
    } else if ( request->argName(i) == F("ntp_svr") ) {
      debugMsg(F("/cmd, setntpcfg received server:"),2);
      debugMsgln(request->arg(i),2);
      ntp_svr = request->arg(i);
    } else if ( request->argName(i) == F("ntp_poll") ) {
      debugMsg(F("/cmd, setntpcfg received poll:"),2);
      debugMsgln(request->arg(i),2);
      ntp_poll = request->arg(i).toInt();
    } else if ( request->argName(i) == F("ntp_tz") ) {
      debugMsg(F("/cmd, setntpcfg received tz:"),2);
      debugMsgln(request->arg(i),2);
      ntp_tz = request->arg(i);
    }
  }
  
  switch (cmd) {
    case read_reg:  MBus_get_reg(addr, value);
                    response_message = value;
                    break;
    case write_reg: MBus_write_reg(addr, data);
                    break;
    case read_coil: bool state;
                    MBus_get_coil(addr, state);
                    (state)?value=F("on"):value=F("off");
                    response_message = value;
                    break;
    case write_coil: MBus_write_coil(addr, data);
                    break;
    case clr_clog:  if(sd_card_available) {
                      debugMsgln(F("Clearing controller log"),1);
                      if (SD.exists(ctlLogFileName)) {
                        ctl_logFile.close();
                        SD.remove(ctlLogFileName);
                        refreshCtlLogFile();
                        response_message = getHTMLHead();
                        response_message += F("Controller log cleared<script>setTimeout(() => { history.back(); }, 1500);</script>");
                        response_message += getHTMLFoot();
                        request->send(200, F("text/html"), response_message);
                        return;
                      }
                    }
                    break;
    case clr_dlog:  if(sd_card_available) {
                      debugMsgln(F("Clearing debug log"),1);
                      if (SD.exists(logFileName)) {
                        logFile.close();
                        SD.remove(logFileName);
                        logFile = SD.open(logFileName,FILE_APPEND);
                        debugMsgln(F("Debug log cleared"),1);
                        logFile.flush();
                        response_message = getHTMLHead();
                        response_message += F("Debug log cleared<script>setTimeout(() => { history.back(); }, 1500);</script>");
                        response_message += getHTMLFoot();
                        request->send(200, F("text/html"), response_message);
                        return;
                      }
                    }  
                    break;
    case set_rtc:   setRtcTime(rtcTime);
                    break;
    case set_aging: setAgingOffset(offset);
                    break;
    case set_wlan:  storeWLANsInEEPROM(ssid, pass, wlan); // /cmd?setwlan=[0-3]&ssid=xxxx&pass=yyyy
                    break;
    case set_rtc_ntp: setRtcTimeNTP();
                    break;
    case cfg_ntp:   if (ntp_svr != "") {
                      ntpServer = ntp_svr;
                      setServer(ntpServer);
                      storeNtpServerInEEPROM(ntpServer);
                    }
                    if (ntp_poll != -1) {
                      if (ntp_poll < 601) ntp_poll = 601;  // shorter is abusive
                      if (ntp_poll > 65535) ntp_poll = 65535; // unsigned short int
                      ntpInterval = ntp_poll;
                      setInterval(ntpInterval);
                      rtc_max_unsync = RTC_MAX_UNSYNC * sqrt(ntpInterval/600);
                      storeNtpPollInEEPROM(ntpInterval);
                    }
                    if (ntp_tz != "") {
                      ntpTZ = ntp_tz;
                      myTZ.setPosix(ntpTZ);
                      storeNtpTZInEEPROM(ntpTZ);
                    }
                    updateNTP();
                    break;
    default:        response_message = F("err");
  }
  request->send(200, F("text/plain"), response_message);
}

void platformPageHandler(AsyncWebServerRequest *request)
/*
 * Returns a page with info on the ESP platform.
 */
{
  debugMsgln(F("Entering /platform page."),2);
  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  #ifdef ARDUINO_ARCH_ESP8266
    response_message += F("<center><img src=\"/img/wemos.png\"></center>");
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    response_message += F("<center><img src=\"/img/wrover.png\"></center>");
  #endif
  
  // Status table
  response_message += getTableHead2Col(F("WLAN Status"), F("Name"), F("Value"));
  if ( WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    IPAddress ip = WiFi.localIP();
    response_message += getTableRow2Col(F("hostname"), my_hostname);
    response_message += getTableRow2Col(F("WLAN IP"), formatIPAsString(ip));
    response_message += getTableRow2Col(F("WLAN MAC"), WiFi.macAddress());
    response_message += getTableRow2Col(F("WLAN SSID"), WiFi.SSID());
  }
    response_message += getTableRow2Col(F("WLAN RSSI"), String(WiFi.RSSI())+" dBm");
  if ( WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    IPAddress softapip = WiFi.softAPIP();
    response_message += getTableRow2Col(F("AP IP"), formatIPAsString(softapip));
    response_message += getTableRow2Col(F("AP MAC"), WiFi.softAPmacAddress());
    #ifdef ARDUINO_ARCH_ESP8266
      response_message += getTableRow2Col(F("AP SSID"), WiFi.softAPSSID());                   
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      response_message += getTableRow2Col(F("AP SSID"), ap_ssid);
    #endif
    response_message += getTableRow2Col(F("AP connections"),String(WiFi.softAPgetStationNum()));
  }
  float pwr = WiFi.getTxPower()/4.0;
  response_message += getTableRow2Col(F("Tx power"), String(pwr)+" dBm" );

  // Make the uptime readable
  long upSecs = millis() / 1000;
  long upDays = upSecs / 86400;
  long upHours = (upSecs - (upDays * 86400)) / 3600;
  long upMins = (upSecs - (upDays * 86400) - (upHours * 3600)) / 60;
  upSecs = upSecs - (upDays * 86400) - (upHours * 3600) - (upMins * 60);
  String uptimeString = ""; uptimeString += upDays; uptimeString += F(" days, "); uptimeString += upHours, uptimeString += F(" hours, "); uptimeString += upMins; uptimeString += F(" mins, "); uptimeString += upSecs; uptimeString += F(" secs");

  response_message += getTableRow2Col(F("Uptime"), uptimeString);
  response_message += getTableRow2Col(F("Version"), SOFTWARE_VERSION);
  response_message += getTableRow2Col(F("Serial Number"), serialNumber);

  response_message += getTableFoot();

  #ifdef ARDUINO_ARCH_ESP8266
    float voltage = ((float)ESP.getVcc() / (float)913) + .005;
    // 895.21 corrects for external 100K pulldown on NodeMCU ADC pin
    // internal voltage divider seems to be about 48.25K / 15.6K
    // this gets to 1% accuracy 2.9-3.5V on test unit
    // varies by unit, 890-920?
    char dtostrfbuffer[15];
    dtostrf(voltage, 7, 2, dtostrfbuffer);
    String vccString = String(dtostrfbuffer);
  #endif
  
  response_message += getTableHead2Col(F("Platform Information"), F("Name"), F("Value"));
  #ifdef ARDUINO_ARCH_ESP8266
    response_message += getTableRow2Col(F("Architecture"), F("ESP8266"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("Architecture"), F("ESP32"));
    response_message += getTableRow2Col(F("Chip revision"), String(ESP.getChipRevision()));
  #endif
  response_message += getTableRow2Col(F("CPU Frequency (MHz)"), String(ESP.getCpuFreqMHz()));
  if (rtcPresent) {
    response_message += getTableRow2Col(F("RTC Time"), myTZ.dateTime(getUnixTime(), UTC_TIME, RFC850));
    response_message += getTableRow2Col(F("RTC Temp"), String(getRtcTemp(), 2) +"&deg;C");
  }
  String datetime = String(__DATE__) + ", " + String(__TIME__) +F(" EST"); //myTZ.dateTime(getUnixTime(), UTC_TIME, RFC850)
  response_message += getTableRow2Col(F("Compiled on"), myTZ.dateTime(compileTime(), RFC850));  //datetime);
  response_message += getTableRow2Col(F("Arduino IDE"), String(ARDUINO));
  response_message += getTableRow2Col(F("Build notes"), F(BUILD_NOTES));
  response_message += getTableRow2Col(F("Sketch size"), formatBytes(ESP.getSketchSize()));
  String ota = formatBytes(ESP.getFreeSketchSpace());
  if ( largeFlash ) { ota += F(" (OTA update capable)"); }
  response_message += getTableRow2Col(F("Free sketch size"), ota);
  
  #ifdef ARDUINO_ARCH_ESP8266
    response_message += getTableRow2Col(F("ESP version"), ESP.getFullVersion());
    response_message += getTableRow2Col(F("Free heap"), formatBytes(ESP.getFreeHeap()));
    response_message += getTableRow2Col(F("Heap fragmentation"), String(ESP.getHeapFragmentation())+" %");
    response_message += getTableRow2Col(F("Stack low watermark"), formatBytes(ESP.getFreeContStack()));

    FSInfo fs_info;
    FILESYSTEM.info(fs_info);
    response_message += getTableRow2Col(fs_type + String(F(" size")), formatBytes(fs_info.totalBytes));
    response_message += getTableRow2Col(fs_type + String(F(" used")), formatBytes(fs_info.usedBytes));
    response_message += getTableRow2Col(fs_type + String(F(" block size")), formatBytes(fs_info.blockSize));
    response_message += getTableRow2Col(fs_type + String(F(" page size")), formatBytes(fs_info.pageSize));
    response_message += getTableRow2Col(fs_type + String(F(" max open files")), String(fs_info.maxOpenFiles));

    response_message += getTableRow2Col(F("Chip ID"), String(ESP.getChipId()));
    response_message += getTableRow2Col(F("Flash Chip ID"), "0x"+String(ESP.getFlashChipId(),HEX));
    response_message += getTableRow2Col(F("Flash size"), formatBytes(ESP.getFlashChipRealSize()));

    extern SpiFlashChip *flashchip;
/* typedef struct{
        uint32  deviceId;
        uint32  chip_size;    // chip size in byte
        uint32  block_size;
        uint32  sector_size;
        uint32  page_size;
        uint32  status_mask;
   } SpiFlashChip;
*/
    response_message += getTableRow2Col(F("Flash block size"), formatBytes(flashchip->block_size));
    response_message += getTableRow2Col(F("Flash sector size"), formatBytes(flashchip->sector_size));
    response_message += getTableRow2Col(F("Flash page size"), formatBytes(flashchip->page_size));

    response_message += getTableRow2Col(F("Last reset reason"), String(ESP.getResetReason()));
    response_message += getTableRow2Col(F("Vcc"), vccString);
  #endif

  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("SDK version"), String(ESP.getSdkVersion()));
    response_message += getTableRow2Col(F("Internal total heap"), String(ESP.getHeapSize()));
    response_message += getTableRow2Col(F("Internal free heap"), String(ESP.getFreeHeap()));
    response_message += getTableRow2Col(F("Internal min free heap"), String(ESP.getMinFreeHeap()));
    response_message += getTableRow2Col(F("SPIFFS size"), formatBytes(SPIFFS.totalBytes()));
    response_message += getTableRow2Col(F("SPIFFS used"), formatBytes(SPIFFS.usedBytes()));
    if (sd_card_available) {
      response_message += getTableRow2Col(F("SD card size"), String(formatBytes(SD.totalBytes())));
      response_message += getTableRow2Col(F("SD card used"), String(formatBytes(SD.usedBytes())));
      if (sd_card_log && sd_card_available && logFile) response_message += getTableRow2Col(F("Debug log file"), \
        "<a href=\"/sd" + logFileName + "\" target=\"_blank\">" + "/sd" + logFileName + "</a>" + \
        "&nbsp;&nbsp;" + \
        "<a href=\"/sd" + logFileName + "\" download>" + "(download)" + "</a>");
      if (sd_card_log && sd_card_available && ctl_logFile) response_message += getTableRow2Col(F("Controller log file"), \
        "<a href=\"/sd" + ctlLogFileName + "\" target=\"_blank\">" + "/sd" + ctlLogFileName + "</a>" + \
        "&nbsp;&nbsp;" + \
        "<a href=\"/sd" + ctlLogFileName + "\" download>" + "(download)" + "</a>");
    }
    response_message += getTableRow2Col(F("SPIRAM total heap"), String(ESP.getPsramSize()));
    response_message += getTableRow2Col(F("SPIRAM free heap"), String(ESP.getFreePsram()));
    response_message += getTableRow2Col(F("SPIRAM min free heap"), String(ESP.getMinFreePsram()));
    response_message += getTableRow2Col(F("Flash chip size"), "0x"+String(ESP.getFlashChipSize(),HEX));
    response_message += getTableRow2Col(F("Flash chip speed"), String(ESP.getFlashChipSpeed()));
    response_message += getTableRow2Col(F("Last reset reason CPU 0"), get_reset_reason(0));
    response_message += getTableRow2Col(F("Last reset reason CPU 1"), get_reset_reason(1));
  #endif

  if (model.startsWith(F("PS-"))) {  // TODO make this universal
    float mbv = 0;
    if (model == "PS-MPPT") {
      MBus_get_float(8, mbv);
    } else if (model == "PS-PWM") {
      MBus_get_float(6, mbv);
    }
    response_message += getTableRow2Col(F("Meterbus Voltage"), String(mbv) + " V");
  }

  response_message += getTableFoot();

  response_message += getHTMLFoot();
  
  debugMsg(F("response_message size:"),4);
  debugMsgln(String(response_message.length()),4);

  #ifdef ARDUINO_ARCH_ESP8266
    // need to buffer in SPIFFs due to low memory on ESP8266
    File tempFile = FILESYSTEM.open(F("/platform.html"), "w");
    tempFile.print(response_message);
    response_message = "";
    tempFile.close();
    request->send(FILESYSTEM, F("/platform.html"), F("text/html"));
    FILESYSTEM.remove(F("/platform.html"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32  
    request->send(200, F("text/html"), response_message);
  #endif
}

void allregsPageHandler(AsyncWebServerRequest *request)
{
  debugMsgln(F("Entering /allregs page."),2);
  String response_message;
  response_message.reserve(8500);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += getTableHead2Col(model+" Registers", F("Register"), F("Value"));
  int response;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  bool eof = false;
  for ( int row = 1; !eof ; row++ ) {
    if (mbRegAddr[row] >= mbRegMax) { eof = true; }    
    String reginfo = mbRegDesc[row] + " [" + String(mbRegAddr[row]) + "]" + " (" + mbRegVar[row] + ")";
    String unit = " " + mbRegUnitName[mbRegUnit[row]];
    debugMsg(F("processing modbus register "),4);
    debugMsgln(String(mbRegAddr[row]),4);
    switch (mbRegType[row]) {
      case f16:     MBus_get_float(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);
                    break;
      case n10:     MBus_get_n10(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);        
                    break;
      case dn10:    MBus_get_dn10(mbRegAddr[row], foo_fl);
                    response_message += getTableRow2Col(reginfo, String(foo_fl) + unit);        
                    break;
      case sigint:  MBus_get_int(mbRegAddr[row], foo_sint);
                    response_message += getTableRow2Col(reginfo, String(foo_sint) + unit);                
                    break;
      case usigint: MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, String(foo_int) + unit);        
                    break;
      case dint:    MBus_get_uint32(mbRegAddr[row], foo_dint);
                    response_message += getTableRow2Col(reginfo, String(foo_dint) + unit);        
                    break;
      case bitfield: MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_int, HEX) + unit);        
                    break;
      case dbitfield: MBus_get_uint32(mbRegAddr[row], foo_dint);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_dint, HEX) + unit);        
                    break;
      case bcd:     MBus_get_uint16(mbRegAddr[row], foo_int);
                    response_message += getTableRow2Col(reginfo, "0x"+String(foo_int, HEX) + unit);        
                    break;
      // TODO additional types (n673 onward)             
      default:           ;
    }
    delayMicroseconds(1000); // brief pause between reading registers works best
  }
  response_message += getTableFoot();

  response_message += getHTMLFoot();
  #ifdef ARDUINO_ARCH_ESP8266
    // need to buffer in SPIFFs due to low memory on ESP8266
    File tempFile = FILESYSTEM.open(F("/allregs.html"), "w");
    tempFile.print(response_message);
    response_message = "";
    tempFile.close();
    request->send(FILESYSTEM, "/allregs.html", F("text/html"));
    //FILESYSTEM.remove(F("/allregs.html"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32  
    request->send(200, F("text/html"), response_message);
  #endif
}


void allcoilsPageHandler(AsyncWebServerRequest *request) {
  debugMsgln(F("Entering /allcoils page."),2);
  String response_message;
  response_message.reserve(3000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  int response;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  bool eof = false;

  response_message += getTableHead2Col(model+" Coils", F("Coil"), F("State"));
  eof = false;
  bool state;
  String coilInfo;
  for ( int row = 1; row <= COIL_ROWS && !eof ; row++ ) {
    if (mbCoilAddr[row] >= mbCoilMax) { eof = true; }
    debugMsg(F("processing modbus coil "),4);
    debugMsgln(String(mbCoilAddr[row]),4);
    MBus_get_coil(mbCoilAddr[row], state);
    coilInfo = mbCoilDesc[row] + " [" + String(mbCoilAddr[row]) + "]" + " (" + mbCoilVar[row] + ")";
    response_message += getTableRow2Col(coilInfo, String(state));
    delayMicroseconds(1000);
  }
  response_message += getTableFoot();
  
  response_message += getHTMLFoot();
  request->send(200, F("text/html"), response_message);
}

/**
   WLAN page allows users to set the WiFi credentials
*/
void wlanPageHandler(AsyncWebServerRequest *request)
{
  String ssid, pass;
  debugMsgln(F("Entering /wlan_config page."),2);

  // Check if there are any GET parameters, if there are, we are configuring
  if (request->hasArg(F("ssid"))) {
      ssid = request->arg("ssid");
      WiFi.persistent(true);
    debugMsgln(F("Configuring WiFi"),2);
    debugMsg(F("New SSID entered:"),2);
    debugMsgln(ssid,2);
    
    if (request->hasArg(F("password")))  {
      pass = request->arg(F("password"));
      debugMsg(F("New PASSWORD entered:"),4);
      debugMsg(pass,9);
      debugMsgln("",4);
    }

    debugMsg("ssid length:",2);
    debugMsgln(String(strlen(ssid.c_str())),2);
    debugMsg("pass length:",2);
    debugMsgln(String(strlen(pass.c_str())),2);
    
    if (connectToWLAN(ssid.c_str(), pass.c_str())) {                // try to connect
      storeWLANsInEEPROM(ssid, pass, 0);                      //save in slot 0 if we did
      debugMsgln("",2);
      debugMsgln(F("WiFi connected"),1);
      debugMsg(F("IP address: "),1);
      debugMsgln(formatIPAsString(WiFi.localIP()),1);
      debugMsg(F("SoftAP IP address: "),1);
      debugMsgln(formatIPAsString(WiFi.softAPIP()),1);
      delay(50);
      reboot();                                                     // and reboot to pick up the new config
    } else {
      debugMsgln("",1);
      debugMsgln(F("WiFi connect failed."),1);
    }
  } // end, got an SSID to configure

  debugMsgln("Starting WiFi scan.",4);
  // Get number of visible access points
  int ap_count = WiFi.scanNetworks();

  String response_message;
  response_message.reserve(3000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  // form header
  response_message += getFormHead(F("Set Configuration"));


  response_message += getDropDownHeader(F("WiFi:"), F("ssid"), true);

  if (ap_count == 0)
  {
    response_message += getDropDownOption(F("-1"), F("No wifi found"), true);
  }
  else
  {
    // Show access points
    for (uint8_t ap_idx = 0; ap_idx < ap_count; ap_idx++)
    {
      String ssid = String(WiFi.SSID(ap_idx));
      String wlanId = String(WiFi.SSID(ap_idx));
      #ifdef ARDUINO_ARCH_ESP8266      
        (WiFi.encryptionType(ap_idx) == ENC_TYPE_NONE) ? wlanId += "" : wlanId += F(" (requires password)");
      #endif 
      #ifdef ARDUINO_ARCH_ESP32
        (WiFi.encryptionType(ap_idx) == WIFI_AUTH_OPEN) ? wlanId += "" : wlanId += F(" (requires password)");
      #endif 
      wlanId += F(" (RSSI: ");
      wlanId += String(WiFi.RSSI(ap_idx));
      wlanId += F(")");

      debugMsg(F("Found ssid: "),3);
      debugMsgln(WiFi.SSID(ap_idx),3);
      if ((esid[0] == ssid)) {                             //TODO multiwifi, push like stack (discard oldest)
      } else {
        debugMsgln(F("IsCurrent: N"),3);
      }
      
      response_message += getDropDownOption(ssid, wlanId, (esid[0] == ssid));
    }

    response_message += getDropDownFooter();

    response_message += getTextInput(F("WiFi password (if required)"), F("password"), "", false);
    response_message += getSubmitButton(F("Set"));

    response_message += getFormFoot();
  }

/*  //TODO - input for 4 SSIDs, but maybe just keep 4 most recents
  for (int i = 0 ; i < 3 ; i++ ) { 
      String esidvar[3];
      String epasvar[3];
      esidvar[i] = "<input type=\"number\" id=\"ssid";
      esidvar += String(i);
      esidvar += "\" size=\"9\" value=\"";
      esidvar += esid[i];                                                       // needs work on local.js
      esidvar += "\" onchange=\"setwlan('";                   //storeWLANsInEEPROM
      esidvar += "this.value, '";
      
      esidvar += String(i);
      esidvar += ")\">&nbsp";
      response_message += getTableRow2Col(esidvar[i], "........" );
  }
*/
  response_message += getHTMLFoot();

  debugMsg(F("response_message size:"),4);
  debugMsgln(String(response_message.length()),4);

  request->send(200, F("text/html"), response_message);

}

/**
   Utility functions
*/
void utilityPageHandler(AsyncWebServerRequest *request)
{
  debugMsgln(F("Entering /utility page."),2);

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<center><img src=\"/img/utility.png\"></center>");

  response_message += F("<br><br><div class=\"container\" role=\"secondary\"><br>");
  response_message += F("<p><hr><h3>Utility Functions</h3>");
  response_message += F("<font size=\"4\">");

  response_message += F("<hr><a href=\"/wlan_config\">Wireless settings</a>");
  response_message += F("<hr><a href=\"/setTime\">Time settings</a>");
  response_message += F("<hr><a href=\"/documentation.htm\">Documentation</a>");
//  response_message += F("<hr><a href=\"/edit\">File edit/view/upload (ctrl-s saves file)</a>");
  response_message += F("<hr><a href=\"/allregs\">Show all registers</a>");
  response_message += F("<hr><a href=\"/allcoils\">Show all coils</a>");
  response_message += F("<hr><a href=\"/rest?json={%22addr%22:255%2c%22cmd%22:");
  response_message += F("%22writeSingleCoil%22%2c%22valu%22:%22on%22%2c%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22%2c%22back%22:%22true%22}\">Restart solar controller</a>");


  response_message += F("<hr><a href=\"/getfile\">Check controller and reread files</a>");
  response_message += F("<hr><a href=\"/reset\">Restart WLAN module</a>");
  if ( largeFlash ) {
    response_message += F("<hr><a href=\"");
    response_message += update_path;
    response_message += F("\">Update WLAN module firmware</a>");
  }

  response_message += F("<hr><h3>Use with caution!</h3>");

  response_message += F("<hr><a href=\"/cmd?clr_dlog\">Clear debug log</a>");
  response_message += F("<hr><a href=\"/cmd?clr_clog\">Clear controller log</a>");
  response_message += F("<hr><a href=\"/rest?json={%22addr%22:254%2c%22cmd%22:");
  response_message += F("%22writeSingleCoil%22%2c%22valu%22:%22on%22%2c%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22%2c%22back%22:%22true%22}\">Reset solar controller to factory defaults</a>");
  response_message += F("<hr><a href=\"/resetall\">Clear config and restart WLAN module</a>");

  response_message += F("</font></div>");
  response_message += getHTMLFoot();

  debugMsg(F("response_message size:"),4);
  debugMsgln(String(response_message.length()),4);

  request->send(200, F("text/html"), response_message);
}

/**
   Reset the ESP card
*/
void getfilePageHandler(AsyncWebServerRequest *request) {
  debugMsgln(F("Entering /getfile page."),2);

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Checking controller and rereading definition file.</strong>");
  checkController();
  if (model=="") { 
    noController = true;
    model = getModelFromEEPROM();
      if (model == "") {
    model = "PS-MPPT";
  }
  debugMsg(F("EEPROM got model:"),1);
  debugMsgln(model,1);
  } else {
    debugMsg(F("Got model from mbus:"),1);
    debugMsgln(model,1);
    noController = false;
  }
  getFile(model);
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 3000);</script>");  
  response_message += getHTMLFoot();
  request->send(200, F("text/html"), response_message);
}


/**
   Reset the EEPROM and stored values
*/
void resetAllPageHandler(AsyncWebServerRequest *request) {
  debugMsgln(F("Entering /resetall page."),2);

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Success!</strong> Reset done.</div>");
  response_message += F("<div class=\"alert alert-success fade in\">Attempting reboot, but power cycle if needed. ");
  response_message += F("You will then need to connect to the <b>"); 
  response_message += String(ap_ssid);
  response_message += F("</b> SSID and open <b>http://192.168.4.1</b> in a web browser to reconfigure.</div></div>");
  response_message += getHTMLFoot();
  resetEEPROM();
  /*
   * system_restore() - Reset to default settings of the following APIs : 
   * wifi_station_set_auto_connect, wifi_set_phy_mode, wifi_softap_set_config related, 
   * wifi_station_set_config related, and wifi_set_opmode.
   */
  system_restore(); // this wipes ESP saved wifi stuff
  
  request->send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
//    server.handleClient();
    delayMicroseconds(1000);    // wait to deliver response
    yield();
  }
  WiFi.disconnect();
  reboot();
}

void resetPageHandler(AsyncWebServerRequest *request) {
  debugMsgln(F("Entering /reset page."),2);

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Attempting a restart.</strong>");
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 12000);</script>");  
  response_message += getHTMLFoot();
  request->send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
//    server.handleClient();
    delayMicroseconds(1000);    // wait to deliver response
    yield();
  }
  reboot();
}

void setTimePageHandler(AsyncWebServerRequest *request) {
 //   Page to set RTC.
  debugMsgln(F("Entering /setTime page."),2);
  String serverArg = "";

  if (request->hasArg("tzname")) {          // for POSIX lookup
    serverArg = String(request->arg("tzname").c_str());
    if ( serverArg.length() > 3 && serverArg.length() < 64 ) {
      tzName = request->arg("tzname").c_str();
      if (myTZ.setLocation(tzName)) {  // also sets TZ string, so we restore after
        tzPosix = myTZ.getPosix();
        myTZ.setPosix(ntpTZ);          // restore the existing string
      }
    }
  }
  
  checkController();
  String response_message, inputvar;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<center><img src=\"/img/setTime.png\"></center>");
  response_message += getFormHead("Clock status");

  if (rtcPresent) {
  response_message += F("The RTC (Real Time Clock) uses a battery to keep time while the controller is not connected to power. ");
  }
  response_message += F("NTP (Network Time Protocol) can retrieve the current time if a connection to the Internet is available. <br><br>");
  
  if (rtcPresent) {  
    response_message += getTextInput(F("Current RTC time"), F("rtc_time"), myTZ.dateTime(getUnixTime(), UTC_TIME, RFC850), true);
    response_message += F("<br><br>");
    response_message += getTextInput(F("RTC last set"), F("rtc_lastset"), myTZ.dateTime(getRtcLastSetTime(), UTC_TIME, RFC850), true);
    response_message += F("<br><br>");
  } else {
    response_message += getTextInput(F("RTC not available"), F("rtc_avail"), "", true);
    response_message += F("<br><br>");
  }
  
  if (timeStatus() == timeSet) {  // ntp got time
    response_message += getTextInput(F("Current NTP time"), F("ntp_time"), myTZ.dateTime(RFC850), true);
    response_message += F("<br><br>");
    response_message += getTextInput(F("Last NTP update"), F("ntp_update"), myTZ.dateTime(lastNtpUpdateTime(), UTC_TIME, RFC850), true);
    response_message += F("<br><br>");
  } else if (timeStatus() == timeNeedsSync) {
    response_message += getTextInput(F("NTP status"), F("ntp_status"), F("Needs sync"), true);
    response_message += F("<br><br>");
  } else if (timeStatus() == timeNotSet) {
    response_message += getTextInput(F("NTP status"), F("ntp_status"), F("Time not set by NTP"), true);
    response_message += F("<br><br>");
  }
  if (rtcNeedsTime && timeStatus() != timeSet) {
    response_message += F("No time source.<br><br>");
  }  
  if (rtcPresent) {
    if (timeStatus() == timeSet) {                // ntp has current time
      response_message += getJsButton(F("Set RTC from NTP"), F("setRtcTimeNTP()"));
    }
    response_message += getJsButton(F("Set RTC from computer"), F("setRtcTime()"));
    response_message += F("<br><br>");
  }
  response_message += getFormFoot();

//getTextInput(String heading, String input_name, String value, boolean disabled)
//getNumberInput(String heading, String input_name, int minVal, int maxVal, int value, boolean disabled)
  response_message += getFormHead("Configure NTP");
  response_message += F("The NTP poll time should <i>not</i> be set to a multiple of 60 seconds. That will help spread \
                        out the load on the NTP servers. 7201 seconds is good, 7200 is not. If using an ntp.org pool \
                        server, polls should happen no more often than every 1/2 hour (1801 seconds) to comply with \
                        their terms of service. <br><br>");
  response_message += getTextInput(F("NTP server"), F("ntp_svr"), ntpServer, false);
  response_message += F("<br><br>");
  String foo = F("Poll interval<br>(");
  foo += String(NTP_MIN_INTERVAL);
  foo += "-";
  foo += String(NTP_MAX_INTERVAL);
  foo += F(" sec)");
  response_message += getNumberInput(foo, F("ntp_poll"), 601, 64999, ntpInterval, false);
  response_message += "<br><br>";
  response_message += getTextInput(F("POSIX timezone string"), F("ntp_tz"), ntpTZ, false);
  response_message += F("<br><br>");
  response_message += getJsButton(F("Update NTP settings"), F("setNtpCfg()"));
  response_message += F("<br><br>");
  response_message += getFormFoot();

  if (rtcPresent && !rtcNeedsTime) {
    response_message += getFormHead("RTC speed adjustment");
    response_message += F("The RTC crystal will be trimmed automatically over time if connected to the Internet and using NTP. \
                          If not using NTP, it can be set by hand here. Each unit is about 3 seconds per year (0.1 ppm). \
                          The factory default is 0. \
                          The offset is where RTC time is compared to NTP, negative means it's fast (ahead of NTP). \
                          When it's more than &plusmn;");
    response_message += String(rtc_max_unsync);
    response_message += F(" ms and stable, an automatic trim adjustment will be made.<br>\
                          Since last set, the RTC is running about ");
    float my_ppm = getRtcppm();
    if (my_ppm < 0) {
      my_ppm *= -1.0;  // abs() doesn't seem to work right on ESP8266, this does
      response_message += String(my_ppm,3);
      response_message += F(" ppm slow compared to NTP.<br><br>");
    } else if (my_ppm > 0) {
      response_message += String(my_ppm,3);
      response_message += F(" ppm fast compared to NTP.<br><br>");
    } else {
      response_message += F("the same as NTP.<br><br>");
    }
    response_message += getTextInput(F("RTC ms offset"), F("rtc_diff_filtered"), String(rtc_diff_filtered), true);
    response_message += F("<br><br>");
    response_message += getNumberInput(F("RTC crystal trim"), F("rtc_offset"), -127, 127, getAgingOffset(), false);
    response_message += F("<br><div>(&plusmn;127 - lower makes RTC faster and the offset above decrease)</div><br>");
    response_message += getJsButton(F("Set crystal trim"), F("setAging('rtc_offset')"));
    response_message += F("<br><br>");
    response_message += getFormFoot();
  }
  
  response_message += getFormHead(F("POSIX timezone lookup"));
  response_message += F("You can try a POSIX lookup by entering an ");
  response_message += F("<a href=\"https://en.wikipedia.org/wiki/List_of_tz_database_time_zones\" target=\"_blank\">");
  response_message += F("Olson name</a> (like America/Detroit) here, and then copy/paste it to the field above. ");
  response_message += getTextInput(F("Olson name"), F("tzname"), tzName, false);
  response_message += F("<br><br>");
  response_message += getTextInput(F("POSIX TZ string:"), F("tzposix"), tzPosix, true);
  response_message += F("<br><br>");
  response_message += getSubmitButton(F("Lookup"));
  response_message += F("<br><hr/>");
  response_message += F("<a href=\"http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html\" target=\"_blank\">");
  response_message += F("POSIX time zone string reference,</a> e.g. US Eastern is <b>EST+5EDT,M3.2.0/2,M11.1.0/2</b>. ");
  response_message += F("<br><hr/>");
  response_message += getFormFoot();
  
  response_message += getHTMLFoot();

  debugMsg(F("response_message size:"),4);
  debugMsgln(String(response_message.length()),4);

  #ifdef ARDUINO_ARCH_ESP8266
    // need to buffer in SPIFFs due to low memory on ESP8266
    File tempFile = FILESYSTEM.open(F("/setTime.htm"), "w");
    tempFile.print(response_message);
    response_message = "";
    tempFile.close();
    request->send(FILESYSTEM, F("/setTime.htm"), F("text/html"));
    FILESYSTEM.remove(F("/setTime.htm"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32  
    request->send(200, F("text/html"), response_message);
  #endif

}

#ifdef ARDUINO_ARCH_ESP32
  bool loadFromSdCard(String path, AsyncWebServerRequest *request) {
    debugMsgln("loadFromSdCard, file:"+path,4);
    String dataType = "text/plain";
    if (path.endsWith("/")) {
      path += "index.htm";
    }
  
    if (path.endsWith(".src")) {
      path = path.substring(0, path.lastIndexOf("."));
    } else if (path.endsWith(".htm")) {
      dataType = "text/html";
    } else if (path.endsWith(".css")) {
      dataType = "text/css";
    } else if (path.endsWith(".js")) {
      dataType = "application/javascript";
    } else if (path.endsWith(".png")) {
      dataType = "image/png";
    } else if (path.endsWith(".gif")) {
      dataType = "image/gif";
    } else if (path.endsWith(".jpg")) {
      dataType = "image/jpeg";
    } else if (path.endsWith(".ico")) {
      dataType = "image/x-icon";
    } else if (path.endsWith(".xml")) {
      dataType = "text/xml";
    } else if (path.endsWith(".pdf")) {
      dataType = "application/pdf";
    } else if (path.endsWith(".zip")) {
      dataType = "application/zip";
    }

    if (logFile) logFile.flush();      // flush logs
    if (ctl_logFile) ctl_logFile.flush();
    #ifdef EZT_DEBUG
      if (ezt_logFile) ezt_logFile.flush();
    #endif

    File dataFile = SD.open(path.c_str());
    if (dataFile.isDirectory()) {
      path += "/index.htm";
      dataType = "text/html";
      dataFile = SD.open(path.c_str());
    }
  
    if (!dataFile) {
      debugMsgln("File not found",4);
      return false;
    }
  
    if (request->hasArg("download")) {
      dataType = "application/octet-stream";
    }
    debugMsgln("Sending file",4);
    request->send(SD, path, dataType);
  
    dataFile.close();
    return true;
  }
  
  void sdPageHandler(String URI, AsyncWebServerRequest *request ){
    debugMsgln("sdPageHandler URI:"+URI,4);
    String relative_uri = URI;
    relative_uri.replace("/sd/","/");
  /*  checkController();
    String response_message;
    response_message.reserve(4000);
    response_message = getHTMLHead();
    response_message += getNavBar();
    response_message += getFormHead(F("SD"));
  
    response_message += "<div>";  
    response_message += "sdPageHandler requested URI:/sd"+relative_uri;
    response_message += "</div>";
  
    response_message += getFormFoot();
    response_message += getHTMLFoot();
    request->send(200, F("text/html"), response_message);
  */
    if (sd_card_available && loadFromSdCard(relative_uri, request)) {
      return;
    }
    String message = "SDCARD Not Detected\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nrelative URI: ";
    message += relative_uri;
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += request->args();
    message += "\n";
    for (uint8_t i = 0; i < request->args(); i++) {
      message += " NAME:" + request->argName(i) + "\n VALUE:" + request->arg(i) + "\n";
    }
    request->send(404, "text/plain", message);
    debugMsgln(message,5);
  }
#endif //esp32
