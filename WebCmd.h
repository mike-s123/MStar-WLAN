// handles js posts

void cmdPageHandler(AsyncWebServerRequest *request) {                          
/*
 *  Handles POST requests made to /cmd
 *  Used by JavaScript
 */
  int addr, offset, slot, numArgs = request->args();
  unsigned short int ntp_poll = -1;
  String data, value, ssid, psk, response_message = F("OK"), rtcTime, ntp_item, \
          ntp_svr = "", ntp_tz = "", var = "", where, pass, hostname;
  string response_msg[10];
  bool wlanPsk=false, wlanSsid=false;
  enum commands { read_reg, write_reg, read_coil, write_coil, set_rtc, set_aging, set_wlan, \
                  set_rtc_ntp, cfg_ntp, clr_dlog, clr_clog, clr_crrd, set_apssid, \
                  set_appsk, set_hostname, set_debuglvl, set_logfreq, set_celsius, set_sn};
  commands cmd;
  debugMsg(F("SET args received: "),4);
  debugMsgln(String(numArgs),4);
  for (int i=0 ; i<numArgs ; i++) {
    debugMsgln("SET arg#"+String(i)+", "+request->argName(i)+": "+request->arg(i),4);
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
    } else if ( request->argName(i) == F("clr_crrd") ) {
      cmd = clr_crrd;
    } else if ( request->argName(i) == F("setrtc") ) {
      cmd = set_rtc;
      rtcTime = request->arg(i);
    } else if ( request->argName(i) == F("setagingoffset") ) {
      cmd = set_aging;
      offset = request->arg(i).toInt();
    } else if ( request->argName(i) == F("setwlan") ) {
      cmd = set_wlan;
      slot = request->arg(i).toInt();
    } else if ( request->argName(i) == F("setapssid") ) {
      cmd = set_apssid;
      ssid = request->arg(i);
    } else if ( request->argName(i) == F("setappsk") ) {
      cmd = set_appsk;
      pass = request->arg(i);
    } else if ( request->argName(i) == F("setdebuglvl") ) {
      cmd = set_debuglvl;
      value = request->arg(i);
    } else if ( request->argName(i) == F("setlogfreq") ) {
      cmd = set_logfreq;
      value = request->arg(i);
    } else if ( request->argName(i) == F("sethostname") ) {
      cmd = set_hostname;
      hostname = request->arg(i);
    } else if ( request->argName(i) == F("data") ) {
      data = request->arg(i);
    } else if ( request->argName(i) == F("ssid") ) {
      wlanSsid = true;
      ssid = request->arg(i);
    } else if ( request->argName(i) == F("psk") ) {
      wlanPsk = true;
      psk = request->arg(i);
    } else if ( request->argName(i) == F("pass") ) {
      pass = request->arg(i);
    } else if ( request->argName(i) == F("setrtcntp") ) {
      cmd = set_rtc_ntp;
    } else if ( request->argName(i) == F("setntpcfg") ) {
      debugMsgln(F("/cmd, setntpcfg received"),2);
      cmd = cfg_ntp;
      ntp_item = request->arg(i);
    } else if ( request->argName(i) == F("ntp_svr") ) {
      debugMsg(F("/cmd, setntpcfg received server: "),2);
      debugMsgln(request->arg(i),2);
      ntp_svr = request->arg(i);
    } else if ( request->argName(i) == F("ntp_poll") ) {
      debugMsg(F("/cmd, setntpcfg received poll: "),2);
      debugMsgln(request->arg(i),2);
      ntp_poll = request->arg(i).toInt();
    } else if ( request->argName(i) == F("ntp_tz") ) {
      debugMsg(F("/cmd, setntpcfg received tz: "),2);
      debugMsgln(request->arg(i),2);
      ntp_tz = request->arg(i);
    } else if ( request->argName(i) == F("celsius") ) {
      debugMsg(F("/cmd, celsius received: "),2);
      debugMsgln(request->arg(i),2);
      cmd = set_celsius;
      var = request->arg(i);
    } else if ( request->argName(i) == F("set_sn") ) {
      debugMsg(F("/cmd, set s/n: "),2);
      debugMsgln(request->arg(i),2);
      cmd = set_sn;
      serialNumber = request->arg(i);
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
                      debugMsgln(F("Clearing controller log"),3);
                      if (SD.exists(ctlLogFileName)) {
                        ctl_logFile.close();
                        SD.remove(ctlLogFileName);
                      }
                      refreshCtlLogFile();
                      debugMsgln(F("Controller log cleared"),1);
                      response_message = getHTMLHead();
                      response_message += F("Controller logs cleared<script>setTimeout(() => { history.back(); }, 1000);</script>");
                      response_message += getHTMLFoot();
                      request->send(200, F("text/html"), response_message);
                      return;
                    }
                    break;
    case clr_crrd:  if(sd_card_available) {
                      response_message = getHTMLHead();
                      if (strcmp(ctlRrdFileName.c_str(), TOSTRING(CTLRRDFILENAME))) {  // only clear if not the default file (strcmp false if equal)
                        if (SD.exists(ctlRrdFileName.c_str())) {
                          debugMsg(F("Clearing controller rrd " ),1);
                          debugMsgln(ctlRrdFileName.c_str(),1);
                          unlink(ctlRrdFileFullPath.c_str());
                          SD.remove(ctlRrdFileName.c_str());
                          unlink(ctlDRrdFileFullPath.c_str());
                          SD.remove(ctlDRrdFileName.c_str());
                        };  
                        refreshCtlLogFile();
                        response_message += F("Controller rrd cleared<script>setTimeout(() => { history.back(); }, 1000);</script>");
                      } else {
                        response_message += F("Default rrd " TOSTRING(CTLRRDFILENAME) " left in place<script>setTimeout(() => { history.back(); }, 1500);</script>"); 
                      }
                      response_message += getHTMLFoot();
                      request->send(200, F("text/html"), response_message);
                      return;
                    }
                    break;
    case clr_dlog:  if(sd_card_available) {
                      debugMsgln(F("Clearing platform log"),3);
                      if (SD.exists(logFileName)) {
                        logFile.close();
                        SD.remove(logFileName);
                        logFile = SD.open(logFileName,FILE_APPEND);
                        debugMsgln(F("Platform log cleared"),1);
                        logFile.flush();
                        response_message = getHTMLHead();
                        response_message += F("Platform log cleared<script>setTimeout(() => { history.back(); }, 1500);</script>");
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
    case set_wlan:  putWLANs(ssid, psk, slot, wlanSsid, wlanPsk); // /cmd?setwlan=[0-3]&ssid=xxxx&psk=yyyy
                    break;
    case set_apssid: ap_SSID = ssid.c_str();
                    preferences.begin(PREF_REALM);
                    preferences.putString("ap_SSID",ap_SSID);
                    preferences.end();
                    break;
    case set_appsk: ap_password = pass.c_str();
                    preferences.begin(PREF_REALM);
                    preferences.putString("ap_password",ap_password);
                    preferences.end();
                    break;
    case set_debuglvl: debug_level = value.toInt();
                    debugMsg(F("Debug level set to: "),1);
                    debugMsgln(String(debug_level),1);
                    preferences.begin(PREF_REALM);
                    preferences.putUInt("debug_level",debug_level);
                    preferences.end();
                    break;
    case set_logfreq: log_freq = value.toInt(); 
                    debugMsg(F("Log interval set to: "),1);
                    debugMsgln(String(log_freq),1);
                    preferences.begin(PREF_REALM);
                    preferences.putUInt("log_freq",log_freq);
                    preferences.end();
                   break;
    case set_hostname: 
                    my_hostname = hostname.c_str();
                    preferences.begin(PREF_REALM);
                    preferences.putString("my_hostname",my_hostname);
                    preferences.end();
                    break;
    case set_rtc_ntp: setRtcTimeNTP();
                    break;
    case cfg_ntp:   if (ntp_svr != "") {
                      ntpServer = ntp_svr;
                      setServer(ntpServer);
                      putNtpServer(ntpServer);
                    }
                    if (ntp_poll != -1) {
                      if (ntp_poll < 601) ntp_poll = 601;  // shorter is abusive
                      if (ntp_poll > 65535) ntp_poll = 65535; // unsigned short int
                      ntpInterval = ntp_poll;
                      setInterval(ntpInterval);
                      rtc_max_unsync = RTC_MAX_UNSYNC * sqrt(ntpInterval/600);
                      putNtpPoll(ntpInterval);
                    }
                    if (ntp_tz != "") {
                      ntpTZ = ntp_tz;
                      myTZ.setPosix(ntpTZ);
                      putNtpTZ(ntpTZ);
                    }
                    updateNTP();
                    break;
    case set_celsius:   
                    (var == "true") ? celsius = true : celsius = false;
                    preferences.begin(PREF_REALM);
                    preferences.putBool("celsius",celsius);
                    preferences.end();
                    break;
    case set_sn:    preferences.begin(PREF_REALM);
                    preferences.putString("serial_number",serialNumber);
                    preferences.end();
                    break;

    default:        response_message = F("err");
  }
  request->send(200, F("text/plain"), response_message);
}


void rootCmdPageHandler(AsyncWebServerRequest *request) {                          
/*
 *  Handles POST requests made to /rootcmd
 *  Used by JavaScript
 */
  int numArgs = request->args();
  String response_message = F("OK"), where, user, pass;
  string response_msg[10];
  enum commands { set_cred, set_jsonpass };
  commands cmd;
  debugMsg(F("rootcmd SET args received: "),4);
  debugMsgln(String(numArgs),4);
  for (int i=0 ; i<numArgs ; i++) {
    debugMsgln("SET arg#"+String(i)+", "+request->argName(i)+": "+request->arg(i),4);
    if ( request->argName(i) == F("setjsonpass") ) {
      cmd = set_jsonpass;
      pass = request->arg(i);
    } else if ( request->argName(i) == F("setcred") ) {  //setcred=[web|secure]&user=xxxx&pass=xxxx
      cmd = set_cred;
      where = request->arg(i);
    } else if ( request->argName(i) == F("user") ) {
      user = request->arg(i);
    } else if ( request->argName(i) == F("pass") ) {
      pass = request->arg(i);
    }
  }
  
  switch (cmd) {
    case set_jsonpass: 
                      preferences.begin(PREF_REALM);
                      preferences.putString("json_password",pass);
                      json_password = pass;
                      preferences.end();
                      break;
    case set_cred:    preferences.begin(PREF_REALM);
                      if (where == "root") {
                        debugMsgln(F("set_cred root"),5);
                        if (user.length() > 0) {
                          preferences.putString("root_username",user);
                          root_username = user;
                        }
                        if (pass.length() > 0) {
                          preferences.putString("root_password",pass);
                          root_password = pass;
                        }
                      } else if (where == "web") {
                        debugMsgln(F("set_cred web"),5);
                        if (user.length() > 0) {
                          preferences.putString("web_username",user);
                          web_username = user;
                        }
                        if (pass.length() > 0) {
                          preferences.putString("web_password",pass);
                          web_password = pass;
                        }                
                      }
                      preferences.end();
                      break;
    default:          response_message = F("err");
  }
  request->send(200, F("text/plain"), response_message);
}
