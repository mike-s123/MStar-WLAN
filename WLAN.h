
void startAP(const char* ssid, const char* password) {
/*
 * This starts AP only, probably because we had no stored STA info.
 */
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif
  WiFi.mode(WIFI_AP);
  debugMsg(F("Starting AP, SSID \""),1);
  debugMsg(String(ssid),1);
  debugMsg(F("\", pass \""),1);
  debugMsg(String(password),1);
  debugMsgln(F("\""),1);
  if (strlen(password) > 0) {
    WiFi.softAP(ssid, password, 6, false, 8);
  } else {
    WiFi.softAP(ssid);
  }
}


boolean connectToWLAN(const char* ssid = "", const char* password = "") {
/*
 *  Try to connect as a station with the given info. 
 *  If no info provided, read stored info from eeprom. Give up after a while
 *  if we can't get in. Returns connected status.
*/
  int retries = 0;
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP,0);  // needs 2.5.0
  #endif

  WiFi.persistent(true);
  if (strlen(ssid)>0) {         // here we try to connect using the info passed to us
    wlan_count++ ;
    if (password && strlen(password) > 0 ) {
      debugMsg(F("wifiMulti adding SSID:"),3);
      debugMsgln(ssid,3);
      debugMsg(F("wifiMulti   with pass:"),9);
      debugMsgln(password,9);
      wifiMulti.addAP(ssid, password);        //esid.c_str()?
    } else {
        wifiMulti.addAP(ssid);
    }
  } else {                        // if we weren't given info, look it up
    getWLANsFromEEPROM();
    int i;
    for (i = 0; i<=3; i++) {
      if (esid[i] != "") {
        if ( epass[i] != "" ) {   // got both ssid and pass
          if (!wlanSet) { // skip if already there
            debugMsg(F("wifiMulti adding SSID from EEPROM:"),3);
            debugMsgln(esid[i],3);
            debugMsg(F("wifiMulti   with pass from EEPROM:"),3);
            debugMsg(epass[i],9);
            debugMsgln("",3);
            wifiMulti.addAP(esid[i].c_str(), epass[i].c_str());
            wlan_count++ ;
          }
        } else {  // only ssid, no pass
          if (!wlanSet) {                  // skip if already there
            debugMsg(F("wifiMulti adding SSID from EEPROM:"),3);
            debugMsgln(esid[i],3);
            wifiMulti.addAP(esid[i].c_str());
            wlan_count++ ;
          }
        }
      }
    }  // done getting WLANs
    wlanSet = true;
  }  // else, didn't get passed WLAN info
  
  #ifdef WIFI_MODE_AP_STA           // mode controlled by #define
    WiFi.mode(WIFI_AP_STA);
    if (strlen(ap_password) > 0) {
      WiFi.softAP(ap_ssid, ap_password, 6, false, 8);
    } else {
      WiFi.softAP(ap_ssid);
    }
  #else
    WiFi.mode(WIFI_STA);
    debugMsgln(F("WLAN changing to station mode."),1);
  #endif
  
  if (wlan_count) {    // if we have WLANs configured for station mode, try to connect
    tryWLAN();
  } else {  // no wlan_count
    debugMsgln(F("No WLANs configured"),1);
    return false;      
  }
  return true;
}

/* 
 *  If we have configured WLANs and are not connected as a station, try to connect every few minutes
 *  Only if running as both AP and STA (doesn't take AP down, but will be non-responsive to client
 *  while scanning). If running as STA -or- AP, don't scan if client is connected.
 */
void tryWLAN() {
  
  #ifdef WIFI_AP_STA  // if AP_STA mode, ok to try with client connected
    if ( wlan_count && !WiFi.isConnected() ) { 
  #else               // if not, would disconnect client, so check
    if ( wlan_count && !WiFi.isConnected() && !WiFi.softAPgetStationNum()  ) {
  #endif
    
    debugMsgln(F("Trying WLAN connection"),2);
    int retries=0;
    #ifndef WIFI_MODE_AP_STA                       // we're in AP mode, switch to try
      debugMsgln(F("WLAN switching to STA"),2);
      WiFi.mode(WIFI_STA);
    #endif
  
    debugMsg(F("Trying to connect to WLAN ("),2);
    debugMsg(String(wlan_count),2);
    debugMsg(")",2);  
    
    while ( wifiMulti.run() != WL_CONNECTED ) {
      delay(500);
      debugMsg(".",2);
      retries++;
      if (retries > 5) {   // try for a while
        debugMsgln("",2);
        debugMsgln(F("Failed to connect"),2);
        #ifndef WIFI_MODE_AP_STA                       // go back to AP mode so users can connect
          debugMsgln(F("WLAN switching back to AP"),2);
          startAP(ap_ssid, ap_password);
        #endif
        return;
      }
    } // while
    debugMsgln("",1);
    IPAddress ip = WiFi.localIP();
    debugMsg(F("WLAN IP address:"),1);
    debugMsgln(formatIPAsString(ip),1);
    debugMsgln("Connected to:" + String(WiFi.SSID()),1);
  }
} // tryWLAN()
