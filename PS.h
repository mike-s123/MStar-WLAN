/*
 * This handles status, charging and other pages for Prostar controllers (and no controller)
 */

void psStatusPageHandler (AsyncWebServerRequest *request) {
/*  
 *   Returns a page of basic controller status.
 */
  int numArgs = request->args();
  if ( numArgs == 0 ) debugMsgln(F("Entering Prostar /status page."),2);
  fullReg reg;
  int result=0;
  String response_message;
// 56,57 alarms
// 47 load fault
// 34 array fault
  String registers[] = {F("adc_vb_f_1m"), F("adc_ib_f_1m"), F("vb_ref"), F("T_batt"), F("adc_va"), F("adc_ia"), \
                        F("adc_il"), F("Ahc_daily"), F("Ahl_daily"), F("time_ab_daily"), F("time_fl_daily"), \
                        F("time_eq_daily"), F("Vb_min_daily"), F("Vb_max_daily"), F("adc_vl"), F("Va_max_daily")};
  //int regsNum[] = {24, 25, 36, 27, 19, 17, 22, 67, 68, 73, 75, 74, 65, 66, 20, 76 };
  String labels[] = {"", "", "Target: ", "", "", "", "", "", "", "Abs: ", "Float: ", "Eq: ", "Min: ", "Max: ", "", "Max: "};
    
  checkController();
  response_message.reserve(8000);

  if ( request->hasArg(F("json")) ) {  //Check if command received
    bool ok = false;
    DynamicJsonDocument jsonIn(3000);
    DeserializationError jsonDesErr = deserializeJson(jsonIn, request->arg(F("json")).c_str());
    DynamicJsonDocument jsonOutDoc(8000);
    JsonObject jsonOut = jsonOutDoc.to<JsonObject>();
    String cmd = jsonIn[F("cmd")];
    if ( cmd == F("readRegs") ) {
      int count = 0;
      JsonArray arr = jsonIn["registers"];
      if (!jsonDesErr) {
        jsonOut[F("model")] = model;
        jsonOut[F("api")] = json_version.c_str();
        jsonOut[F("api_min")] = json_version_min.c_str();
        count = arr.size();
        JsonArray regArray = jsonOut.createNestedArray(F("registers"));
        for (JsonVariant elem : arr ) {
          int mbReg;
          JsonVariant jsonReg = elem["register"];
          if (jsonReg.is<int>()) {
            mbReg = jsonReg.as<int>();
          } else {
            String regName = jsonReg.as<String>();
            mbReg = findAddrByVar(regName);  // look up register number
          }  
          int mbRegIdx = getMbRegIndex(mbReg);
          if ( mbRegIdx >= 0 ) {
            String valu;
            uint16_t raw;
            JsonObject regO      = regArray.createNestedObject();
            JsonObject registers = regO.createNestedObject(F("register"));
            registers[F("addr")]     = mbReg;
            registers[F("name")]     = mbRegVar[mbRegIdx];
            registers[F("desc")]     = mbRegDesc[mbRegIdx];
            MBus_get_reg(mbReg, valu); 
            registers[F("valu")]     = valu;
            registers[F("unit")]     = mbRegUnitName[mbRegUnit[mbRegIdx]];
            registers[F("type")]     = mbRegTypeName[mbRegType[mbRegIdx]];
            MBus_get_reg_raw(mbReg, raw);
            if ( mbRegType[mbRegIdx] == dint || mbRegType[mbRegIdx] == dbitfield ||  mbRegType[mbRegIdx] == dn10 ) {  // only 32 bit datatypes ?
              registers[F("vrhi")]   = "0x"+String(raw, HEX);      
              MBus_get_reg_raw(mbReg+1, raw);
              registers[F("vrlo")]   = "0x"+String(raw,HEX);
            } else {
              registers[F("vraw")]   = "0x"+String(raw,HEX);
            }
            ok = true;
          }
        }
      } else { // parsing failed
        jsonErr(request, F("parsing failed"), F("bad request") , 400); return; 
      }
      if (ok) {  
        response_message = "";
        serializeJsonPretty(jsonOut,response_message);
        request->send(200, F("application/json"), response_message);
      } else {
        jsonErr(request, F("general error"), F("server error"), 500);
      }
    } // end readregs 
    return;
  } // end json
  
  response_message = getHTMLHead();
  response_message += getNavBar();


  response_message += F("<div class=\"controller\"><img src=\"");
  if ( model == "") {
    response_message += F("/ctl/Nocontroller.png");    
  } else {
    response_message += "/ctl/" + model + ".png";
  }
  response_message += F("\" alt=\"controller\">");
  float adc_pa = 0;
  bool gotFirst = false;
  for ( int i=0 ; (i < 16) && !noController ; i++) {
    if ( registers[i] == F("adc_va") || registers[i] == F("adc_ia") ) { // Array power is different between PWM and MPPT models
      int addy;
      addy = findAddrByVar(registers[i]);
      MBus_get_float(addy, vary);
      if (gotFirst) {
        adc_pa = adc_pa * vary;
        if (model.startsWith(F("PS-MPPT"))) {
          response_message += getStatusItemDiv("Sweep_Pmax", "PMax:"); // MPPT - Array Max Output Power (found during sweep)
        } else {
          response_message += F("<div id=\"Sweep_Pmax\">");  // PWM - calculate solar power
          response_message += String(adc_pa);
          response_message += F(" W</div>");
        }
        gotFirst = false;
      } else {
        adc_pa = vary;
        gotFirst = true;
      }
    } 
    response_message += getStatusItemDiv(registers[i], labels[i]);  // items are placed on graphic via <div> and local.css
  }
  response_message += F("</div>"); 

  response_message += getTableHead2Col(F("Battery"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 24);  // Batt V
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>" );
  result = mbGetFullReg(reg, 36);  // Batt target V
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 25);  // Batt A
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 27);  // Batt Temp
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 65);  // Batt minv
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 66);  // Batt maxv
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  response_message += getTableFoot();
 
  response_message += getTableHead2Col(F("Charging"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 67);  // Ah charge daily
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 73);  // Time abs
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 75);  // Time float
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>" );
  result = mbGetFullReg(reg, 74);  // Time eq
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Solar"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 19);  // Array V
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  float aVolts = reg.value.toFloat();
  result = mbGetFullReg(reg, 17);  // Array A
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  float aWatts = reg.value.toFloat() * aVolts ;
  if (model.startsWith(F("PS-MPPT"))) {
    result = mbGetFullReg(reg, 62); // Array Pmax
    response_message += getTableRow2Col(F("Max Watts (sweep)"), "<div id=\"Sweep_Pmax-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  } else {
    response_message += getTableRow2Col(F("Array Watts"), "<div id=\"Sweep_Pmax-body\">" + String(aWatts) + " W</div>");  
  }
  result = mbGetFullReg(reg, 76);  // Array MaxV
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Load"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 20);  // Load V
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 22);  // Load A
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  result = mbGetFullReg(reg, 68);  // Load Ah
  response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");

  response_message += getTableFoot();

  response_message += F("<script src=\"/ctl/PSupdateStatus.js\"></script>");

  response_message += getHTMLFoot();

  debugMsg(F("response_message size:"),7);
  debugMsgln(String(response_message.length()),7);

  request->send(200, F("text/html"), response_message);

}


void psSetChargePageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set controller charging settings.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering PS /setcharge page."),2);

  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  
  response_message += F("<center><img src=\"/img/charging.png\" alt=\"Charging\"></center>");  
  String heading = F("Charge Settings <font size=\"4\">(changes have immediate effect)</font>");
  heading += "<h2 id=\"warning\"></h2>";

  response_message += getTableHead2Col(heading, F("Description"), F("Value")); 

  // TODO - make sure this is standardized
  String registers[] = {F("EV_reg"), F("Et_float"), F("EV_float"), F("EV_floatlb_trip"), F("Et_floatlb"), F("EV_eq"), \
                      F("Et_eq_reg"), F("Et_eq_above"), F("EV_float_cancel"),F("Et_float_exit_cum"), \
                      F("Evb_ref_lim"), F("Evb_ref_charge_lim"), F("Et_eqcalendar"), \
                      F("EV_tempcomp"), F("ETb_max"), F("ETb_min") };

  for (int i = 0 ; i < 16 ; i++ ) { 
    addr = findAddrByVar(registers[i]);
    if (addr >= 0) {
      fullReg reg;
      String value;
      mbGetFullReg(reg, addr);
      String inputvar = "<input type=\"number\" id=\"";
      inputvar += reg.var;
      inputvar += "\" size=\"9\" value=\"";
      inputvar += reg.value;
      inputvar += "\" onchange=\"writeReg('";
      inputvar += addr;
      inputvar += "', this.value, '";
      inputvar += reg.var;
      inputvar += "')\">&nbsp";
      response_message += getTableRow2Col(reg.var+" ("+reg.desc+")", inputvar + mbRegUnitName[reg.unit] );
    }
  }

  response_message += getTableFoot();
  response_message += getHTMLFoot();
  debugMsg(F("response_message size:"),7);
  debugMsgln(String(response_message.length()),7);
  request->send(200, F("text/html"), response_message);
}

void psSetOtherPageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering /setother page."),2);
   
  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<center><img src=\"/img/otherset.png\" alt=\"Other\"></center>");
  String heading = F("Other Settings <font size=\"4\">(changes have immediate effect)</font>");
  heading += "<h2 id=\"warning\"></h2>";

  response_message += getTableHead2Col(heading, F("Description"), F("Value")); 

  String registers[] = {F("EV_lvd"), F("EV_lvr"), F("EV_hvd"), F("EV_hvr"), F("EV_lhvd"), F("EV_lhvr"), \
                        F("ER_icomp"), F("Et_lvd_warn"), \
                        F("EV_soc_g_gy"), F("EV_soc_gy_y"), F("EV_soc_y_yr"), F("EV_soc_yr_r") };

  for (int i = 0 ; i < 12 ; i++ ) { 
    addr = findAddrByVar(registers[i]);
    if (addr >= 0) {
      fullReg reg;
      String value;
      mbGetFullReg(reg, addr);
      String inputvar = "<input type=\"number\" id=\"";
      inputvar += reg.var;
      inputvar += "\" size=\"9\" value=\"";
      inputvar += reg.value;
      inputvar += "\" onchange=\"writeReg('";
      inputvar += addr;
      inputvar += "', this.value, '";
      inputvar += reg.var;
      inputvar += "')\">&nbsp";
      response_message += getTableRow2Col(reg.var+" ("+reg.desc+")", inputvar + mbRegUnitName[reg.unit] );
    }
  }

  response_message += getTableFoot();
  response_message += getHTMLFoot();

  debugMsg(F("response_message size:"),7);
  debugMsgln(String(response_message.length()),7);

  request->send(200, F("text/html"), response_message);
}

int psGetSn() {
  char serialNum[10] = "00000000";
  int snStart = findAddrByVar("sn12");
  if (snStart > 0) {
    uint16_t sn;  
    for (int i; i<4; i++) {
      MBus_get_reg_raw(snStart+i, sn);
      serialNum[i*2] = sn % 256;
    serialNum[(i*2)+1] = sn / 256;
    }
    serialNum[8] = 0 ;
  } else {
    debugMsgln(F("No serial number register found"),1);
    return 0;
  }
  ctlSerialNum = serialNum;
  return 1;
}

void psGetCtlLogFileName() {
  if (ctlSerialNum == "00000000") psGetSn(); // try twice if needed
  ctlLogFileName = "/ctl" + ctlSerialNum + ".csv" ;
  debugMsgln("ctlLogFileName set to:"+ctlLogFileName, 1);
}

void psLog() {

  /*
   *   Battery V (18,adc_vb_f_1m)
   *   Net A (25,adc_ib_f_1m)
   *   Target V (36,vb_ref)
   *   Battery temp (27,T_batt)
   *   Array V (19,adc_va)
   *   Array C (17,adc_ia)
   *   Load V (20,adc_vl)
   *   Load C (22,adc_il)
   *   State (Abs/Fl/Eq) (33,charge_state)
   *     0 START
   *     1 NIGHT_CHECK
   *     2 DISCONNECT
   *     3 NIGHT
   *     4 FAULT
   *     5 BULK          (MPPT)
   *     6 ABSORPTION    (
   *     7 FLOAT
   *     8 EQUALIZE
   *     9 SLAVE
   *     10 FIXED
   */
  int registers[] = { 18,25,36,27,19,17,20,22,33 };
  int numReg = sizeof(registers)/sizeof(registers[0]);
  fullReg reg;
  String logLine="";
  logLine.reserve(120);
  for (int i=0; i<numReg; i++) {
    mbGetFullReg(reg, registers[i]);
    if (reg.value == "-0.00") { // can happen for very small negative values
      logLine += "0.00";
    } else {
      logLine += reg.value;
    }
    if (registers[i] == 36) {
      if (reg.value.toFloat() >= 0.1 && !daytime) {
          debugMsgln(F("Start of daytime."),1);
          daytime = true;
      }
      if (reg.value.toFloat() < 0.1 && daytime) {
        debugMsgln(F("Start of nighttime."),1);
        daytime = false;
      }
    }
    if (i+1 < numReg) logLine += ",";
  }
  if (model.startsWith(F("PS-MPPT"))) {
    mbGetFullReg(reg, 62); // Array Max Output Power (found during sweep)
    logLine += "," + reg.value;
  } 
  logLine += ",0,0,0";  // Spares 1-3, reserved for future use, 
                        // because MSView complains if the number of headers doesn't exactly match 
                        // the number of csv fields. This lets us add new fields in the future and 
                        // still have the existing log file still work with MSView.
  if (logLine != logLast && logLine.indexOf(F("err")) < 0 ) { // only write an entry if something has changed and no error
    ctl_logFile.print(myTZ.dateTime(ISO8601)+","); // ECMAScript/ISO8601: YYYY-MM-DDTHH:mm...
    ctl_logFile.println(logLine);
    logLast = logLine;
  }
}

void PSopenLogFile() {
  debugMsgln("Closing controller log: "+ctlLogFileName,1);
  ctl_logFile.close(); // release hooks and buffers
  getCtlLogFileName();
  //if (sd_card_available)
  if (SD.exists(ctlLogFileName)) {
    ctl_logFile = SD.open(ctlLogFileName,FILE_APPEND);  // already there, just open
  } else {                                              // doesn't exist, so open and write header for MS-View
    ctl_logFile = SD.open(ctlLogFileName,FILE_APPEND);
    ctl_logFile.print(F("\"Time (local)\",\"Batt V\",\"Net A\",\"Target V\",\"Batt Temp C\",\"Solar V\",\"Solar A\","));
    ctl_logFile.print(F("\"Load V\",\"Load A\",\"Charge State\""));
    if (model.startsWith(F("PS-MPPT"))) {
      ctl_logFile.print(F(",\"Solar Max W\""));   // Array Max Output Power (found during sweep)
    }
    ctl_logFile.print(F(",\"Spare1\",\"Spare2\",\"Spare3\""));
    ctl_logFile.println();
  }
  debugMsgln("Opened controller log: "+ctlLogFileName,1);
  server.serveStatic("/controller.csv",SD,ctlLogFileName.c_str()); 
}
