/*
 * This handles status, charging and other pages for Prostar controllers (and no controller)
 */



void psStatusPageHandler () {
/*  
 *   Returns a page of basic controller status.
 */
  debugMsgln(F("Entering Prostar /status page."),1);
  fullReg reg;
  int result;
  String response_message;
    
  checkController();

  response_message.reserve(4000);
  response_message = getHTMLHead(30);
  response_message += getNavBar();

// 56,57 alarms
// 47 load fault
// 34 array fault
// TODO make this configurable

  String registers[] = {F("adc_vb_f_1m"), F("adc_ib_f_1m"), F("vb_ref"), F("T_batt"), F("adc_va"), F("adc_ia"), \
                        F("adc_il"), F("Ahc_daily"), F("Ahl_daily"), F("time_ab_daily"),F("time_fl_daily"), \
                        F("time_eq_daily"), F("Vb_min_daily"), F("Vb_max_daily"), F("adc_vl"), F("Va_max_daily") };
  String labels[] = {"", "", "Target: ", "", "", "", "", "", "", "Abs: ", "Float: ", "Eq: ", "Min: ", "Max: ", "", "Max: "};

  response_message += F("<div class=\"controller\"><img src=\"");
  if ( model == "") {
    response_message += F("/ctl/Nocontroller.png");    
  } else {
    response_message += "/ctl/" + model + ".png";
  }
  response_message += F("\" alt=\"controller\">");
  float adc_pa = 0;
  for ( int i=0 ; (i < 16) && !noController ; i++) {
    if ( registers[i] == F("adc_va") || registers[i] == F("adc_ia") ) { //handle power calculation
      int addy;
      float vary;
      addy = findAddrByVar(registers[i]);
      MBus_get_float(addy, vary);
      if (adc_pa) {
        adc_pa = adc_pa * vary;
        response_message += F("<div id=\"adc_pa\">");
        response_message += String(adc_pa);
        response_message += F(" W</div>");
      } else {
        adc_pa = vary;
      }
    } 
    response_message += getStatusItem(registers[i], labels[i]);  // items are placed on graphic via local.css
  }
  response_message += F("</div>"); 

  response_message += getTableHead2Col(F("Battery"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 24);  // Batt V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 36);  // Batt target V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 25);  // Batt A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 27);  // Batt Temp
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 65);  // Batt minv
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 66);  // Batt maxv
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  response_message += getTableFoot();
 
  response_message += getTableHead2Col(F("Charging"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 67);  // Ah charge daily
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 73);  // Time abs
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  result = mbGetFullReg(reg, 75);  // Time float
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  result = mbGetFullReg(reg, 74);  // Time eq
  response_message += getTableRow2Col(reg.desc, secToMin(reg.value) + " m");
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Solar"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 19);  // Array V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  float aVolts = reg.value.toFloat();
  result = mbGetFullReg(reg, 17);  // Array A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  float aWatts = reg.value.toFloat() * aVolts ;
  response_message += getTableRow2Col(F("Array Watts"), String(aWatts) + " W");  
  result = mbGetFullReg(reg, 76);  // Array MaxV
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  response_message += getTableFoot();

  response_message += getTableHead2Col(F("Load"), F("Register"), F("Value"));
  result = mbGetFullReg(reg, 20);  // Load V
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 22);  // Load A
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);
  result = mbGetFullReg(reg, 68);  // Load Ah
  response_message += getTableRow2Col(reg.desc, reg.value + " " + mbRegUnitName[reg.unit]);

  response_message += getTableFoot();

  response_message += getHTMLFoot();
  server.send(200, F("text/html"), response_message);

}


void psSetChargePageHandler() {
/*  
 *   Page to set controller charging settings.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering PS /setcharge page."),1);

  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  
  response_message += F("<center><img src=\"/img/charging.png\"></center>");  
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
  server.send(200, F("text/html"), response_message);
}

void psSetOtherPageHandler() {
/*  
 *   Page to set settings other than charging.
 */
  int addr, result;
  String desc, val;
  debugMsgln(F("Entering /setother page."),1);
   
  checkController();

  String response_message;
  response_message.reserve(4000);
  response_message = getHTMLHead();
  response_message += getNavBar();

  response_message += F("<center><img src=\"/img/otherset.png\"></center>");
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
  server.send(200, F("text/html"), response_message);
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
   *     5 BULK
   *     6 ABSORPTION
   *     7 FLOAT
   *     8 EQUALIZE
   */
  int registers[] = { 18,25,36,27,19,17,20,22,33 };
  int numReg = sizeof(registers)/sizeof(registers[0]);
  fullReg reg;
  String logLine="";
  static String logLast="";
  logLine.reserve(100);
  for (int i=0; i<numReg; i++) {
    mbGetFullReg(reg, registers[i]);
    logLine+=String(reg.value);
    if (i+1 < numReg) logLine += ",";
  }
  if (logLine != logLast) { // only write an entry if something has changed
    #ifdef ARDUINO_ARCH_ESP32
      ctl_logFile.print(UTC.dateTime(F("Y-m-d\\TH:i\\Z"))+","); //ECMAScript: YYYY-MM-DDTHH:mmZ
      ctl_logFile.println(logLine);
    #endif
    logLast = logLine;
  }
}
