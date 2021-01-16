/*
 * This handles status, charging and other pages for Prostar controllers (and no controller)
 */

float dailyMaxP=0; // accumulates the max solar power generated

// forward declarations
void updatePsLogDaily();
// end forward declarations

void psStatusPageHandler (AsyncWebServerRequest *request) {
/*  
 *   Returns a page of basic controller status.
 */
  int numArgs = request->args();
  if ( numArgs == 0 ) debugMsgln(F("Entering Prostar /status page."),3);
  fullReg reg;
  int result=0;
  String response_message((char *)0);
  response_message.reserve(8000);
// 56,57 alarms
// 47 load fault
// 34 array fault
  String registers[] = {F("adc_vb_f_1m"), F("adc_ib_f_1m"), F("vb_ref"), F("T_batt"), F("adc_va"), F("adc_ia"), \
                        F("adc_il"), F("Ahc_daily"), F("Ahl_daily"), F("time_ab_daily"), F("time_fl_daily"), \
                        F("time_eq_daily"), F("Vb_min_daily"), F("Vb_max_daily"), F("adc_vl"), F("Va_max_daily")};
  //int regsNum[] = {24, 25, 36, 27, 19, 17, 22, 67, 68, 73, 75, 74, 65, 66, 20, 76 };
  String labels[] = {"", "", "Target: ", "", "", "", "", "", "", "Abs: ", "Float: ", "Eq: ", "Min: ", "Max: ", "", "Max: "};
    
  checkController();
  
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
            if ( mbRegVar[mbRegIdx] == "T_batt" && !celsius) {
              registers[F("valu")]    = String(((valu.toFloat() * 9.0/5.0)+32.0),1); // convert to Fahrenheit
              registers[F("unit")]    = "&deg;F";
            } else {
              registers[F("valu")]     = valu;
              registers[F("unit")]     = mbRegUnitName[mbRegUnit[mbRegIdx]];
            }
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
  if ( noController ) {
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
    if ( registers[i] == F("T_batt") && !celsius) {
      result = mbGetFullReg(reg, findAddrByVar("T_batt"));
      String foo = "<div id=\"T_batt\">";
      foo += String(((reg.value.toFloat() * 9.0/5.0)+32.0),1);
      foo += " &deg;F</div>";
      response_message += foo;
    } else {
      response_message += getStatusItemDiv(registers[i], labels[i]);  // items are placed on graphic via <div> and local.css
    }
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
  if (celsius) {
    response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + reg.value + " " + mbRegUnitName[reg.unit] + "</div>");
  } else {
    response_message += getTableRow2Col(reg.desc, "<div id=\"" + reg.var + "-body\">" + String(((reg.value.toFloat() * 9.0/5.0)+32.0),1) + "&deg; F</div>");    
  }
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

  debugMsg(F("response_message size: "),3);
  debugMsgln(String(response_message.length()),3);

  request->send(200, F("text/html"), response_message);

}

void psSetChargePageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set controller charging settings.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering PS /setcharge page."),3);

  checkController();

  String response_message((char *)0);
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  
  response_message += F("<center><img src=\"/img/charging.png\" alt=\"Charging\"></center>");  
  String heading = F("Charge Settings <font size=\"4\">(changes have immediate effect)</font>");
  heading += "<h2 id=\"warning\"></h2>";

  response_message += getTableHead2Col(heading, F("Description"), F("Value")); 

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
  debugMsg(F("response_message size: "),3);
  debugMsgln(String(response_message.length()),3);
  request->send(200, F("text/html"), response_message);
}

void psSetOtherPageHandler(AsyncWebServerRequest *request) {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering PS /setother page."),3);
   
  checkController();

  String response_message((char *)0);
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

  debugMsg(F("response_message size: "),3);
  debugMsgln(String(response_message.length()),3);

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

bool createPsRrd() {
  string fullpath = sd_vfs_path + ctlRrdFileName ;
  const char* path = (char*)fullpath.c_str() ; // Must use full vfs path
  String foo = String(getMidnight()-86400); // midnight yesterday
  const char* today = foo.c_str();
  const int argc = 20; // must match, or will fault
  const char* rrd_0[argc] =  {"rrd_create",         //Argv[0] will be ignored
                      path,
                      "--step", "60",               // seconds
                      "--start", today,
                      "DS:BattV:GAUGE:305:0:36",    // range is 0-36, we'll wait a bit over 5 minutes
                      "DS:NetA:GAUGE:305:-40:40",   // range is -40-40
                      "DS:TargetV:GAUGE:305:0:36",  // range is 0-36
                      "DS:BatTemp:GAUGE:305:-30:60", // range is -30-60
                      "DS:ArrayV:GAUGE:305:0:125",  // range is 0-125
                      "DS:ArrayC:GAUGE:305:-1:45",  // range is -1-45
                      "DS:LoadV:GAUGE:305:0:36",    // range is 0-36
                      "DS:LoadC:GAUGE:305:-1:40",   // range is -1-40
                      "DS:MaxP:GAUGE:305:-10:1200",  // range is -10-1200
                      "DS:State:GAUGE:305:0:7",     // range is 0-7
                      "RRA:AVERAGE:0.5:1:2880",     // 2 days @ 1 minute
                      "RRA:AVERAGE:0.5:15:2688",    // 4 weeks @ 15 minutes
                      "RRA:AVERAGE:0.5:60:3650",    // 5 months @ 1 hour
                      "RRA:AVERAGE:0.5:360:2920"};  // 2 years @ 6 hours
  // 2d@1m, 4w@15m, 5M@1h, 2y@6h ~= 977152 (955K) byte file, suitable for 1 MB FRAM? TODO
                      
  char junk_str[9] = "rrd_info";
  char* checker0[2] = {junk_str, (char*)fullpath.c_str()};
  if (access(fullpath.c_str(), F_OK) == -1 || 
        rrd_info(2,(char**)checker0) == NULL) { // did not return any info
    unlink(fullpath.c_str());
    debugMsg("createRrd: ",2);
    debugMsgln(fullpath.c_str(),2);
    if(rrd_create(argc, (char**)rrd_0) != 0) {
      debugMsgln("Unable to create rrd",1);
      return 1;
    } else {
      debugMsg("rrd file created: ",1);
      debugMsgln(fullpath.c_str(),1);
    }
  }
  return 0;
}  

bool createPsDRrd() {  // for Daily rrd
  string fullpath = sd_vfs_path + ctlDRrdFileName ;
  const char* path = (char*)fullpath.c_str() ; // Must use full vfs path
  String foo = String(getMidnight()-86400); // midnight yesterday
  const char* today = foo.c_str();
  const int argc = 16;                                                                                  // must match, or will fault
  const char* rrd_0[argc] =  {"rrd_create",         //Argv[0] will be ignored
                      path,
                      "--step", "86400",              // 1 day
                      "--start", today,
                      "DS:MinBattV:GAUGE:90000:9:36",
                      "DS:MaxBattV:GAUGE:90000:9:36",
                      "DS:ChargeAh:GAUGE:90000:0:400",
                      "DS:LoadAh:GAUGE:90000:0:400",
                      "DS:MaxArrV:GAUGE:90000:0:125",
                      "DS:AbsorpT:GAUGE:90000:0:86400",
                      "DS:EqT:GAUGE:90000:0:86400",
                      "DS:FloatT:GAUGE:90000:0:86400",
                      "DS:MaxPD:GAUGE:90000:0:1200",
                      "RRA:AVERAGE:0.5:1:3650"};    // 10 years @ 1 day
                      
  char junk_str[9] = "rrd_info";
  char* checker0[2] = {junk_str, (char*)fullpath.c_str()};
  if (access(fullpath.c_str(), F_OK) == -1 || 
        rrd_info(2,(char**)checker0) == NULL) { // did not return any info
    unlink(fullpath.c_str());
    debugMsg("createDRrd: ",2);
    debugMsgln(fullpath.c_str(),2);
    if(rrd_create(argc, (char**)rrd_0) != 0) {
      debugMsgln("Unable to create Drrd",1);
      return 1;
    } else {
      debugMsg("daily rrd file created: ",1);
      debugMsgln(fullpath.c_str(),1);
    }
  }
  return 0;
}  

void psGetCtlLogFileName() {
  if (ctlSerialNum == "00000000") psGetSn(); // try twice if needed
  ctlLogFileName = "/ctl" + ctlSerialNum + ".csv" ;
  debugMsgln("ctlLogFileName set to: "+ctlLogFileName, 2);
  String rrdtmp =  "/ctl" + ctlSerialNum + ".rrd";
  ctlRrdFileName = rrdtmp.c_str();
  ctlRrdFileFullPath = sd_vfs_path + ctlRrdFileName;
  debugMsg("ctlRrdFileFullPath set to: ", 2);
  debugMsgln(ctlRrdFileFullPath.c_str(), 2);
  rrdtmp =  "/ctl" + ctlSerialNum + "d.rrd";
  ctlDRrdFileName = rrdtmp.c_str();
  ctlDRrdFileFullPath = sd_vfs_path + ctlDRrdFileName;
  debugMsg("ctlDRrdFileFullPath set to: ", 2);
  debugMsgln(ctlDRrdFileFullPath.c_str(), 2);
}

void psLogDaily(bool record = false) {
  debugMsgln("psLogDaily",2);
  /*
   *     Daily
   *     'Vb_min_daily': 65
   *     'Vb_max_daily': 66
   *     'Ahc_daily': 67
   *     'Ahl_daily': 68
   *     'Va_max_daily': 76
   *     'time_ab_daily': 73
   *     'time_eq_daily': 74
   *     'time_fl_daily': 75
   *     also - MaxPD - computed
   */
  struct log_struct { float MinBattV = 36, MaxBattV = 0, ChargeAh = 0, LoadAh = 0, \
                    MaxArrV = 0; uint16_t AbsorpT = 0, EqT = 0, FloatT = 0; } static log_record;
//  static log_struct log_record = {36,0,0,0,0,0,0,0};
  String value;
  float  f_value, maxPD;
  uint32_t i_value;
  MBus_get_reg(65, value);
  f_value = value.toFloat();
  if (f_value < log_record.MinBattV) log_record.MinBattV = f_value;
  MBus_get_reg(66, value);
  f_value = value.toFloat();
  if (f_value > log_record.MaxBattV) log_record.MaxBattV = f_value;
  MBus_get_reg(67, value);
  f_value = value.toFloat();
  if (f_value > log_record.ChargeAh) log_record.ChargeAh = f_value;
  MBus_get_reg(68, value);
  f_value = value.toFloat();
  if (f_value > log_record.LoadAh) log_record.LoadAh = f_value;
  MBus_get_reg(76, value);
  f_value = value.toFloat();
  if (f_value > log_record.MaxArrV) log_record.MaxArrV = f_value;
  MBus_get_reg(73, value);
  i_value = value.toInt();
  if (i_value > log_record.AbsorpT) log_record.AbsorpT = i_value;
  MBus_get_reg(74, value);
  i_value = value.toInt();
  if (i_value > log_record.EqT) log_record.EqT = i_value;
  MBus_get_reg(75, value);
  i_value = value.toInt();
  if (i_value > log_record.FloatT) log_record.FloatT = i_value;
  MBus_get_reg(19, value); // array V
  f_value = value.toFloat(); // Array C
  MBus_get_reg(17, value);
  f_value = f_value * value.toFloat();
  if (f_value > dailyMaxP) dailyMaxP = f_value;
  
  if (!record) { 
    setEvent(updatePsLogDaily,now()+900);  // do it again in 15 minutes
  } else {  // Save in RRD and stop checking
    deleteEvent(updatePsLogDaily);
    String timeNow = String(getUnixTime());
    string rrd_update = timeNow.c_str();
    rrd_update += ":";
    value = String(log_record.MinBattV);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.MaxBattV);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.ChargeAh);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.LoadAh);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.MaxArrV);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.AbsorpT);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.EqT);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(log_record.FloatT);
    rrd_update += value.c_str();
    rrd_update += ":";
    value = String(dailyMaxP);
    rrd_update += value.c_str();

    char* rrd_up[1] = { (char*)rrd_update.c_str() } ;
    if (nightTime - bootTime < 600 ) {  // we rebooted after nighttime started, probably don't have valid data
      timeNow = String(getUnixTime());
      timeNow += ":U:U:U:U:U:U:U:U:U";  // so report as unknown
      rrd_up[0] = { (char*)timeNow.c_str() };  // C arrays are so weird
    }
    char* my_template = (char*)"MinBattV:MaxBattV:ChargeAh:LoadAh:MaxArrV:AbsorpT:EqT:FloatT:MaxPD" ;
    if (vfs_fat_registered) {
      esp_err_t ret = rrd_update_r(ctlDRrdFileFullPath.c_str(), my_template, 1, (const char**)&rrd_up);
      if (ret != ESP_OK) {
        debugMsg("psLogDaily update failed: ",1);
        debugMsgln(ctlDRrdFileFullPath.c_str(),1);
      } else {
        debugMsg("psLogDaily update: ",2);
        debugMsgln(rrd_update.c_str(),2);        
      }
    }
  } // if (record)
}

void updatePsLogDaily(){              // starts collecting min/max
  psLogDaily(false);
}

void savePsLogDaily(){                // write daily min/max to rrd log
  debugMsgln("savePsLogDaily",2);
  psLogDaily(true);
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
   *     
   */
  debugMsgln("psLog",3);
  struct log_record {float BattV, NetA, TargetV, BattTemp, ArrayV, ArrayC, LoadV, LoadC; int State; float MaxP; int s1, s2, s3;} ;
  //  static log_record log_accumulate[log_freq] = {};
  static log_record * log_accumulate = NULL;
  static unsigned int last_log_freq = 0;
  static int log_record_num = 0;
  if ( last_log_freq != log_freq ) {   //changed
    log_record_num = 0;
    free(log_accumulate);
    if (psRAMavail) {
      log_accumulate = (log_record *) ps_calloc(log_freq, sizeof(log_record));
    } else {
      log_accumulate = (log_record *) calloc(log_freq, sizeof(log_record));
    }
  }
  last_log_freq = log_freq;
  log_record log_consolidated = {};
  String value;
  String logLine="";
  logLine.reserve(120);

  String timeNow = String(getUnixTime());
  string rrd_update = timeNow.c_str();
//  string rrd_update = "N";
  MBus_get_reg(18, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].BattV = value.toFloat();
  MBus_get_reg(25, value);  
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].NetA = value.toFloat();
  MBus_get_reg(36, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].TargetV = value.toFloat();
  MBus_get_reg(27, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].BattTemp = value.toFloat();
  MBus_get_reg(19, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].ArrayV = value.toFloat();
  MBus_get_reg(17, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].ArrayC = value.toFloat();
  MBus_get_reg(20, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].LoadV = value.toFloat();
  MBus_get_reg(22, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_accumulate[log_record_num].LoadC = value.toFloat();
  if (model.startsWith(F("PS-MPPT"))) {
    MBus_get_reg(62, value); // Array Max Output Power (found during sweep)
    rrd_update += ":";
    rrd_update += value.c_str();
    if ( log_accumulate[log_record_num].ArrayC > 0.001 ) {      // MPPT reports last sweep value, even is it's currently dark
      log_accumulate[log_record_num].MaxP = value.toFloat();    // so we only use that value if there's also current from the array
    } else {
      log_accumulate[log_record_num].MaxP = 0;                  // othewise, we'd record the last reported value all night.
    }
    if (log_accumulate[log_record_num].MaxP > dailyMaxP) dailyMaxP = log_accumulate[log_record_num].MaxP;
  } else {
    float p = log_accumulate[log_record_num].ArrayV * log_accumulate[log_record_num].ArrayC;
    log_accumulate[log_record_num].MaxP = p;
    String s = String(p);
    if (p > dailyMaxP) dailyMaxP = p;
    rrd_update += ":";
    rrd_update += s.c_str();
  }
  MBus_get_reg(33, value);
  rrd_update += ":";
  rrd_update += value.c_str();
  log_consolidated.State = value.toInt();  // we don't average this, just use the last value
  if (log_consolidated.State == 3 && daytime) {
    daytime = false;
    if (myTZ.hour() > 12) {
      debugMsgln(F("Start of nighttime."),1);
      nightTime = UTC.now(); // keep track
      updatePsLogDaily(); // start tracking daily stats, not if it's before noon
    }
  }
  if (log_consolidated.State != 3 && !daytime) {
    daytime = true;
    if (myTZ.hour() < 12) debugMsgln(F("Start of daytime."),1);
  }
  char* rrd_up[1] = { (char*)rrd_update.c_str() } ;
  char* my_template = (char*)"BattV:NetA:TargetV:BatTemp:ArrayV:ArrayC:LoadV:LoadC:MaxP:State" ;
  if (vfs_fat_registered) {
    unsigned long tt = millis();
    //esp_err_t ret = rrd_update_r(ctlRrdFileFullPath.c_str(), NULL, 1, (const char**)&rrd_up);
    esp_err_t ret = rrd_update_r(ctlRrdFileFullPath.c_str(), my_template, 1, (const char**)&rrd_up);
    if (ret == ESP_OK) {
      debugMsg("rrd_update_r: ",3);
      debugMsgln(rrd_update.c_str(),3);
      debugMsg("rrd_update_r took (ms): ",4);
      debugMsgln(String(millis()-tt), 4);       // about 91 in testing
    } else {
      debugMsg("rrd_update_r failed to: ",1);
      debugMsgln(ctlRrdFileFullPath.c_str(),1);
    }
  }
  if (++log_record_num < log_freq) return; // we accumulate 
  
  log_record_num = 0; 
  for (int i=0; i < log_freq; i++){  // calculate 15 minute averages
    log_consolidated.BattV += log_accumulate[i].BattV / (float)log_freq;
    log_consolidated.NetA += log_accumulate[i].NetA / (float)log_freq;
    log_consolidated.TargetV += log_accumulate[i].TargetV / (float)log_freq;
    log_consolidated.BattTemp += log_accumulate[i].BattTemp / (float)log_freq;
    log_consolidated.ArrayV += log_accumulate[i].ArrayV / (float)log_freq;
    log_consolidated.ArrayC += log_accumulate[i].ArrayC / (float)log_freq;
    log_consolidated.LoadV += log_accumulate[i].LoadV / (float)log_freq;
    log_consolidated.LoadC += log_accumulate[i].LoadC / (float)log_freq;
    if ( log_accumulate[i].MaxP > log_consolidated.MaxP ) log_consolidated.MaxP = log_accumulate[i].MaxP; // maximum
  }

  logLine = String(log_consolidated.BattV,2) + "," +
            String(log_consolidated.NetA,2) + "," +
            String(log_consolidated.TargetV,2) + "," +
            String(log_consolidated.BattTemp,1) + "," +
            String(log_consolidated.ArrayV,1) + "," +
            String(log_consolidated.ArrayC,1) + "," +
            String(log_consolidated.LoadV,2) + "," +
            String(log_consolidated.LoadC,2) + "," +
            String(log_consolidated.State) + "," +
            String(log_consolidated.MaxP,2) + "," 
            ;
  
  logLine += "0,0,0";   // Spares 1-3, reserved for future use, 
                        // because MSView complains if the number of headers doesn't exactly match 
                        // the number of csv fields. This lets us add new fields in the future and 
                        // still have the existing log file still work with MSView.
  if (logLine != logLast ) { // only write an entry if something has changed and no error
    ctl_logFile.print(myTZ.dateTime(ISO8601)+","); // ECMAScript/ISO8601: YYYY-MM-DDTHH:mm...
    ctl_logFile.println(logLine);
    logLast = logLine;
  }
}

void PSopenLogFile() {
  debugMsgln("Closing controller log: "+ctlLogFileName,1);
  ctl_logFile.close(); // release hooks and buffers
  getCtlLogFileName();
  if (!noController) {
    if (SD.exists(ctlLogFileName)) {
      ctl_logFile = SD.open(ctlLogFileName,FILE_APPEND);  // already there, just open
    } else {                                              // doesn't exist, so open and write header for MS-View
      ctl_logFile = SD.open(ctlLogFileName,FILE_APPEND);
      ctl_logFile.print(F("\"Time (local)\",\"Batt V\",\"Net A\",\"Target V\",\"Batt Temp C\",\"Solar V\",\"Solar A\","));
      ctl_logFile.print(F("\"Load V\",\"Load A\",\"Charge State\""));
      ctl_logFile.print(F(",\"Solar Max W\""));   // Array Max Output Power (found during sweep)
      ctl_logFile.print(F(",\"Spare1\",\"Spare2\",\"Spare3\""));
      ctl_logFile.println();
    }
    debugMsgln("Opened controller log: "+ctlLogFileName,1);
  }
  debugMsg("ctlRrdFileName: ",1);
  debugMsgln(ctlRrdFileName.c_str(),1);
  if (!SD.exists(ctlRrdFileName.c_str()) && vfs_fat_registered)  createPsRrd();
  server.removeHandler(&rrdHandler);
  rrdHandler = server.on("/controller.rrd", HTTP_GET, [](AsyncWebServerRequest *request){
    string path = "/sd"+ctlRrdFileName;
    request->redirect(path.c_str());
  });


  debugMsg("ctlDRrdFileName: ",1);
  debugMsgln(ctlDRrdFileName.c_str(),1);
  if (!SD.exists(ctlDRrdFileName.c_str()) && vfs_fat_registered) createPsDRrd();
  server.removeHandler(&drrdHandler);
  drrdHandler = server.on("/controllerd.rrd", HTTP_GET, [](AsyncWebServerRequest *request){
    string path = "/sd"+ctlDRrdFileName;
    request->redirect(path.c_str());
  });
}
