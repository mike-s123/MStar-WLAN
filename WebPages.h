
//-------------------------------------------------------------------
//------------------------- Page handlers  --------------------------
//-------------------------------------------------------------------

void statusPageHandler () {
/*  
 *   Returns a page of basic controller status.
 */
  #if DEBUG_ON>0
    debugMsg(F("Entering /status page."));
  #endif
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psStatusPageHandler();
  }
}

void setChargePageHandler() {
/*  
 *   Page to set controller charging settings.
 */
  #if DEBUG_ON>0
    debugMsg(F("Entering /setcharge page."));
  #endif
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psSetChargePageHandler();
  }
}

void setOtherPageHandler() {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  #if DEBUG_ON>0
    debugMsg(F("Entering /setother page."));
  #endif   
  checkController();
  if (noController || model.startsWith("PS-")) {  // break out different controller families
    psSetOtherPageHandler();
  }
}

void cmdPageHandler() {                          
/*
 *  Handles POST requests made to /cmd
 *  Used by JavaScript
 */
  int addr, offset, wlan, numArgs = server.args();
  unsigned short int ntp_poll = -1;
  String data, value, ssid, pass, response_message = F("OK"), rtcTime, ntp_item, ntp_svr = "", ntp_tz = "";
  enum commands { read_reg, write_reg, read_coil, write_coil, set_rtc, set_aging, set_wlan, set_rtc_ntp, cfg_ntp };
  commands cmd;
  #if DEBUG_ON>2
    debugMsgContinue(F("SET args received:"));
    debugMsg(String(numArgs));
  #endif  
  for (int i=0 ; i<numArgs ; i++) {
    #if DEBUG_ON>2
      debugMsg("SET arg#"+String(i)+", "+server.argName(i)+":"+server.arg(i));
    #endif
    if ( server.argName(i) == F("writereg") ) {
      cmd = write_reg;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("readreg") ) {
      cmd = read_reg;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("writecoil") ) {
      cmd = write_coil;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("readcoil") ) {
      cmd = read_coil;
      addr = server.arg(i).toInt();
    } else if ( server.argName(i) == F("setrtc") ) {
      cmd = set_rtc;
      rtcTime = server.arg(i);
    } else if ( server.argName(i) == F("setagingoffset") ) {
      cmd = set_aging;
      offset = server.arg(i).toInt();
    } else if ( server.argName(i) == F("setwlan") ) {
      cmd = set_wlan;
      wlan = server.arg(i).toInt();
    } else if ( server.argName(i) == F("data") ) {
      data = server.arg(i);
    } else if ( server.argName(i) == F("ssid") ) {
      ssid = server.arg(i);
    } else if ( server.argName(i) == F("pass") ) {
      pass = server.arg(i);
    } else if ( server.argName(i) == F("setrtcntp") ) {
      cmd = set_rtc_ntp;
    } else if ( server.argName(i) == F("setntpcfg") ) {
      #if DEBUG_ON>1
        debugMsg(F("/cmd, setntpcfg received"));
      #endif  
      cmd = cfg_ntp;
      ntp_item = server.arg(i);
    } else if ( server.argName(i) == F("ntp_svr") ) {
      #if DEBUG_ON>1
        debugMsgContinue(F("/cmd, setntpcfg received server:"));
        debugMsg(server.arg(i));
      #endif  
      ntp_svr = server.arg(i);
    } else if ( server.argName(i) == F("ntp_poll") ) {
      ntp_poll = server.arg(i).toInt();
    } else if ( server.argName(i) == F("ntp_tz") ) {
      ntp_tz = server.arg(i);
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
    case set_rtc:   setRtcTime(rtcTime);
                    break;
    case set_aging: setAgingOffset(offset);
                    break;
    case set_wlan:  storeWLANsInEEPROM(ssid, pass, wlan);
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
                      storeNtpPollInEEPROM(ntpInterval);
                    }
                    if (ntp_tz != "") {
                      ntpTZ = ntp_tz;
                      myTZ.setPosix(ntpTZ);
                      storeNtpTZInEEPROM(ntpTZ);
                    }
                    break;
    default:        response_message = F("err");
  }
  server.send(200, F("text/plain"), response_message);
}

void platformPageHandler()
/*
 * Returns a page with info on the ESP platform.
 */
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /platform page."));
  #endif

  checkController();

  String response_message;
  response_message.reserve(5000);
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
    response_message += getTableRow2Col(F("WLAN RSSI"), String(WiFi.RSSI()));
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
  
  // ESP8266 Info table
  response_message += getTableHead2Col(F("Platform Information"), F("Name"), F("Value"));
  #ifdef ARDUINO_ARCH_ESP8266
    response_message += getTableRow2Col(F("Architecture"), F("ESP8266"));
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("Architecture"), F("ESP32"));
    response_message += getTableRow2Col(F("Chip revision"), String(ESP.getChipRevision()));
  #endif
  response_message += getTableRow2Col(F("CPU Frequency (MHz)"), String(ESP.getCpuFreqMHz()));
  if (useRTC) {
    response_message += getTableRow2Col(F("RTC Time"), myTZ.dateTime(getUnixTime(),RFC850));
    response_message += getTableRow2Col(F("RTC Temp"), String(getRTCTemp(), 2));
  }
  String datetime = String(__DATE__) + ", " + String(__TIME__) +F(" EST");
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
    float mbv = 0.0;
    if (model == "PS-MPPT") {
      MBus_get_float(8, mbv);
    } else if (model == "PS-PWM") {
      MBus_get_float(6, mbv);
    }
    response_message += getTableRow2Col(F("Meterbus Voltage"), String(mbv) + " V");
  #endif

  #ifdef ARDUINO_ARCH_ESP32
    response_message += getTableRow2Col(F("SDK version"), String(ESP.getSdkVersion()));
    response_message += getTableRow2Col(F("Internal total heap"), String(ESP.getHeapSize()));
    response_message += getTableRow2Col(F("Internal free heap"), String(ESP.getFreeHeap()));
    response_message += getTableRow2Col(F("Internal min free heap"), String(ESP.getMinFreeHeap()));
    response_message += getTableRow2Col(F("SPIFFS size"), formatBytes(SPIFFS.totalBytes()));
    response_message += getTableRow2Col(F("SPIFFS used"), formatBytes(SPIFFS.usedBytes()));  
    response_message += getTableRow2Col(F("SPIRAM total heap"), String(ESP.getPsramSize()));
    response_message += getTableRow2Col(F("SPIRAM free heap"), String(ESP.getFreePsram()));
    response_message += getTableRow2Col(F("SPIRAM min free heap"), String(ESP.getMinFreePsram()));
    response_message += getTableRow2Col(F("Flash chip size"), "0x"+String(ESP.getFlashChipSize(),HEX));
    response_message += getTableRow2Col(F("Flash chip speed"), String(ESP.getFlashChipSpeed()));
    response_message += getTableRow2Col(F("Last reset reason CPU 0"), get_reset_reason(0));
    response_message += getTableRow2Col(F("Last reset reason CPU 1"), get_reset_reason(1));
    if (model.startsWith(F("PS-"))) {  // TODO make this universal
      float mbv;
      if (model == "PS-MPPT") {
        MBus_get_float(8, mbv);
      } else if (model == "PS-PWM") {
        MBus_get_float(6, mbv);
      }
      response_message += getTableRow2Col(F("Meterbus Voltage"), String(mbv) + " V");
    }
  #endif
  response_message += getTableFoot();

  response_message += getHTMLFoot();

  server.send(200, F("text/html"), response_message);
}

void allregsPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /allregs page."));
  #endif
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
    #if DEBUG_ON>3
      debugMsgContinue(F("processing modbus register "));
      debugMsg(String(mbRegAddr[row]));
    #endif
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
    delay(1); // brief pause between reading registers works best
  }
  response_message += getTableFoot();

  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}


void allcoilsPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /allcoils page."));
  #endif
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
    #if DEBUG_ON>3
      debugMsgContinue(F("processing modbus coil "));
      debugMsg(String(mbCoilAddr[row]));
    #endif
    MBus_get_coil(mbCoilAddr[row], state);
    coilInfo = mbCoilDesc[row] + " [" + String(mbCoilAddr[row]) + "]" + " (" + mbCoilVar[row] + ")";
    response_message += getTableRow2Col(coilInfo, String(state));
    delay(1);
  }
  response_message += getTableFoot();
  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

/**
   WLAN page allows users to set the WiFi credentials
*/
void wlanPageHandler()
{
  String ssid, pass;
  #if DEBUG_ON>0
    debugMsg(F("Entering /wlan_config page."));
  #endif

  // Check if there are any GET parameters, if there are, we are configuring
  if (server.hasArg(F("ssid"))) {
      ssid = server.arg("ssid");
//    WiFi.persistent(true);
    #if DEBUG_ON>0
      debugMsg(F("Configuring WiFi"));
      debugMsgContinue(F("New SSID entered:"));
      debugMsg(ssid);
    #endif
    
    if (server.hasArg(F("password")))  {
      pass = server.arg(F("password"));
      #if DEBUG_ON>3
      debugMsgContinue(F("New PASSWORD entered:"));
      #endif
      #if DEBUG_ON>8
      debugMsgContinue(pass);
      #endif
      #if DEBUG_ON>3
      debugMsg("");
      #endif
    }

    #if DEBUG_ON>0
    debugMsgContinue("ssid length:");
    debugMsg(String(strlen(ssid.c_str())));
    debugMsgContinue("pass length:");
    debugMsg(String(strlen(pass.c_str())));
    #endif

    
    if (connectToWLAN(ssid.c_str(), pass.c_str())) {                // try to connect
      storeWLANsInEEPROM(ssid, pass, 0);                      //save in slot 0 if we did
      #if DEBUG_ON>0
        debugMsg("");
        debugMsg(F("WiFi connected"));
        debugMsgContinue(F("IP address: "));
        debugMsg(formatIPAsString(WiFi.localIP()));
        debugMsgContinue(F("SoftAP IP address: "));
        debugMsg(formatIPAsString(WiFi.softAPIP()));
      #endif
      delay(50);
      reboot();                                                     // and reboot to pick up the new config
    } else {
      #if DEBUG_ON>0
        debugMsg("");
        debugMsg(F("WiFi connect failed."));
      #endif
    }
  } // end, got an SSID to configure

  String response_message;
  response_message.reserve(3000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  // form header
  response_message += getFormHead(F("Set Configuration"));

  #if DEBUG_ON>3
    debugMsg("Starting WiFi scan.");
  #endif
  // Get number of visible access points
  int ap_count = WiFi.scanNetworks();

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

      #if DEBUG_ON>2
      debugMsgContinue(F("Found ssid: "));
      debugMsg(WiFi.SSID(ap_idx));
      if ((esid[0] == ssid)) {                             //TODO multiwifi, push like stack (discard oldest)
      } else {
        debugMsg(F("IsCurrent: N"));
      }
      #endif
      
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

  server.send(200, F("text/html"), response_message);

}

/**
   Utility functions
*/
void utilityPageHandler()
{
  #if DEBUG_ON>0
    debugMsg(F("Entering /utility page."));
  #endif

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
  response_message += F("<hr><a href=\"/edit\">File edit/view/upload (ctrl-s saves file)</a>");
  response_message += F("<hr><a href=\"/allregs\">Show all registers</a>");
  response_message += F("<hr><a href=\"/allcoils\">Show all coils</a>");
  response_message += F("<hr><a href=\"/rest?json={%22addr%22:255,%22cmd%22:");
  response_message += F("%22writeSingleCoil%22,%22valu%22:%22on%22,%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22,%22back%22:%22true%22}\">Restart solar controller</a>");


  response_message += F("<hr><a href=\"/getfile\">Check controller and reread files</a>");
  response_message += F("<hr><a href=\"/reset\">Restart WLAN module</a>");
  if ( largeFlash ) {
    response_message += F("<hr><a href=\"");
    response_message += UPDATE_PATH;
    response_message += F("\">Update WLAN module firmware</a>");
  }

  response_message += F("<hr><h3>Use with caution!</h3>");
  
  response_message += F("<hr><a href=\"/rest?json={%22addr%22:254,%22cmd%22:");
  response_message += F("%22writeSingleCoil%22,%22valu%22:%22on%22,%22pass%22:%22");
  response_message += json_password;
  response_message += F("%22,%22back%22:%22true%22}\">Reset solar controller to factory defaults</a>");
  response_message += F("<hr><a href=\"/resetall\">Clear config and restart WLAN module</a>");

  response_message += F("</font></div>");
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}

/**
   Reset the ESP card
*/
void getfilePageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /getfile page."));
  #endif

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
  #if DEBUG_ON>0
    debugMsgContinue(F("EEPROM got model:"));
    debugMsg(model);
  #endif
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(F("Got model from mbus:"));
      debugMsg(model);
    #endif
    noController = false;
  }
  getFile(model);
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 3000);</script>");  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}


/**
   Reset the EEPROM and stored values
*/
void resetAllPageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /resetall page."));
  #endif

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
  
  server.send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
    server.handleClient();
    delay(1);    // wait to deliver response
    yield();
  }
  WiFi.disconnect();
  reboot();
}

/**
   Reset the ESP card
*/
void resetPageHandler() {
  #if DEBUG_ON>0
    debugMsg(F("Entering /reset page."));
  #endif

  String response_message;
  response_message.reserve(2000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += F("<div class=\"alert alert-success fade in\"><strong>Attempting a restart.</strong>");
  response_message += F("<script> var timer = setTimeout(function() {window.location='/'}, 12000);</script>");  
  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
  for ( int i = 0; i < 1000 ; i++ ) {
    server.handleClient();
    delay(1);    // wait to deliver response
    yield();
  }
  reboot();
}

void setTimePageHandler() {
 //   Page to set RTC.
  #if DEBUG_ON>0
    debugMsg(F("Entering /setTime page."));
  #endif
    String serverArg = "";

 if (server.hasArg("tzname")) // for POSIX lookup
  {
    serverArg = String(server.arg("tzname").c_str());
    if ( serverArg.length() > 3 && serverArg.length() < 64 ) {
      tzName = server.arg("tzname").c_str();
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
  
  if (useRTC) {  
    response_message += getTextInput(F("Current RTC time"), F("rtc_time"), myTZ.dateTime(getUnixTime(),RFC850), true);
    response_message += F("<br><br>");
  } 
  
  if (timeStatus() == timeSet) {  // ntp got time
    response_message += getTextInput(F("Current NTP time"), F("ntp_time"), myTZ.dateTime(RFC850), true);
    response_message += F("<br><br>");
    response_message += getTextInput(F("Last NTP update"), F("ntp_update"), myTZ.dateTime(lastNtpUpdateTime(), UTC_TIME, RFC850), true);
    response_message += F("<br><br>");
  } else if (!useRTC) {
    response_message += F("<br>No time source.");
  }
  
  if (useRTC) {
    response_message += getTextInput(F("RTC last set"), F("rtc_lastset"), myTZ.dateTime(getRtcLastSetTime(),RFC850), true);
    response_message += F("<br><br>");
    if (timeStatus() == timeSet) {                // ntp has current time
      response_message += F("<br><br>");
      response_message += getJsButton(F("Set RTC from NTP"), F("setRtcTimeNTP()"));
    }
    response_message += getJsButton(F("Set RTC from computer"), F("setRtcTime()"));
    response_message += F("<br><br>");

    response_message += F("The RTC crystal will be trimmed automatically over time if connected to the Internet and using NTP.<br>\
                          If not using NTP, it can be set by hand here. Each unit is about 3 seconds per year (0.1 ppm).<br>\
                          The offset is where RTC time is compared to NTP, negative means it's fast (ahead of NTP).<br>\
                          When it's more than &plusmn;100 ms and stable, an automatic trim adjustment will be made.<br>\
                          Since last set, the RTC is running about ");
    float my_ppm = getRTCppm();
    if (my_ppm < 0) {
      response_message += String(abs(my_ppm),3);
      response_message += F(" ppm slow compared to NTP.<br><br>");
    } else if (my_ppm > 0) {
      response_message += String(abs(my_ppm),3);
      response_message += F(" ppm fast compared to NTP.<br><br>");
    } else {
      response_message += F("the same as NTP.<br><br>");
    }
    response_message += getTextInput(F("RTC ms offset"), F("rtc_diff_filtered"), String(rtc_diff_filtered), true);
    response_message += F("<br><br>");
    response_message += getNumberInput(F("RTC crystal trim"), F("rtc_offset"), -127, 127, getAgingOffset(), false);
    response_message += F("<br><div>(&plusmn;127 - lower makes RTC faster and the offset above decrease)</div><br>");
    response_message += getJsButton(F("Set crystal trim"), F("setAging('rtc_offset')"));
  }
  
  response_message += getFormFoot();
  response_message += getFormHead("Configure NTP");

//getTextInput(String heading, String input_name, String value, boolean disabled)
//getNumberInput(String heading, String input_name, int minVal, int maxVal, int value, boolean disabled)

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
  response_message += getFormFoot();
 
  response_message += getFormHead(F("Configuration Info"));

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
  response_message += F("POSIX time zone string reference,</a> e.g. US Eastern is <b>EST5EDT,M3.2.0,M11.1.0</b>. ");
  response_message += F("<br><hr/>");
  response_message += F("The NTP poll time should <i>not</i> be set to a multiple of 60 seconds. That will help spread ");
  response_message += F("out the load on the NTP servers. 7201 seconds is good, 7200 is not. If using an ntp.org pool ");
  response_message += F("server, polls should happen no more often than every 1/2 hour (1800 seconds) to comply with ");
  response_message += F("their terms of service.");
  response_message += getFormFoot();


  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);
}
