/* 
 *  This page handles rest requests, as documented in REST_API.txt
 *  (in FILESYSTEM).
 * 
 *  Requests and responses are in JSON.
 *  
 */

void jsonErr(String info, String message="bad request", int code=400) {
  String response_message;
  response_message.reserve(1000);
  response_message = F("{\"error\": {\"code\":");
  response_message += String(code);
  response_message += ",\"message\":\""+message+"\",\"info\":\""+info+"\"}}";
  server.send(code, F("application/json"), response_message); 
}

void restPageHandler() {                          //  URI /rest
/* example of use
 * http://192.168.168.151/rest?json={"addr":0,"cmd":"readHoldingRegisters","count":3}
 * err if the given address doesn't exist
 * will return the next "count" (default=1) available registers, 
 * skipping unavailable ones.
*/

  #if DEBUG_ON>0
    debugMsg(F("Entering /rest page."));
  #endif

  bool ok = false;
  if ( server.hasArg(F("json")) ) {  //Check if command received
    StaticJsonBuffer<200> jsonInBuffer;
    JsonObject& jsonIn = jsonInBuffer.parseObject(server.arg(F("json")).c_str());
    String cmd = jsonIn[F("cmd")];
    String pass = jsonIn[F("pass")];
    StaticJsonBuffer<2200> jsonOutBuffer;    // on the edge, anymore than ~2400 and wdt resets
    JsonObject& jsonOut = jsonOutBuffer.createObject();
    if ( cmd == F("readHoldingRegisters") || cmd == F("readInputRegisters") || cmd == F("readInputRegister") ) {
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
      int count = jsonIn[F("count")] | 1;
      if ( cmd == "readInputRegister" ) { count = 1; };
      if ( count > 10 ) { jsonErr(F("count > 10"), F("request entity too large") , 413); return; }
      if ( getMbRegIndex(mbReg) < 0 ){ jsonErr(F("bad addr")); return; }
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version;
      jsonOut[F("api_min")] = json_version_min;
      JsonArray &regArray = jsonOut.createNestedArray(F("registers"));
      for (int i=0; i<count && mbReg <= mbRegMax; mbReg++ ) {
        int mbRegIdx = getMbRegIndex(mbReg);
        if ( mbRegIdx >= 0 ) {
          String valu;
          uint16_t raw;
          JsonObject& regO      = regArray.createNestedObject();
          JsonObject& registers = regO.createNestedObject(F("register"));
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
          i++;
        }
      }
/* example of use
 * http://192.168.168.151/rest?json={"addr":1,"cmd":"readCoils","count":3}
 * will return the next "count" available items, ignoring unavailable ones.
*/
    } else if ( cmd == F("readCoils") || cmd == F("readDiscreteInputs") ) {
      String address = jsonIn[F("addr")] | "-1";
      int mbCoil = getDecInt(address);              // this covers all bases
      int count  = jsonIn[F("count")] | 1;
      if ( count > 10 ) { jsonErr(F("count > 10"), F("request entity too large") , 413); return; }
      if ( getCoilIndex(mbCoil) < 0 ){ jsonErr(F("bad addr")); return; }
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version;
      jsonOut[F("api_min")] = json_version_min;
      JsonArray &coilArray = jsonOut.createNestedArray(F("coils"));
      for (int i=0; i<count && mbCoil <= mbCoilMax; mbCoil++ ) {
        int coilIdx = getCoilIndex(mbCoil);
        bool state;
        if ( coilIdx >= 0 ) {
          JsonObject& coilO  = coilArray.createNestedObject();
          JsonObject& coils  = coilO.createNestedObject(F("coil"));
          coils[F("addr")]  = mbCoil;
          coils[F("name")]  = mbCoilVar[coilIdx];
          coils[F("desc")]  = mbCoilDesc[coilIdx];
          MBus_get_coil(mbCoil, state);
          coils[F("stat")] = state;
          ok = true;
          i++;
        }
      }
    } else if ( cmd == F("writeSingleRegister") ) {
      #if DEBUG_ON>3
        debugMsg(F("json writeSingleRegister"));
      #endif
      String valu="";
      int result;
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
//      int mbReg = jsonIn[F("addr")] | -1;
      int mbRegIndex = getMbRegIndex(mbReg);
      if ( !mbRegRW[mbRegIndex] ) { jsonErr(F("read only")); return; }
      valu = jsonIn[F("pass")] | "none";
      if ( valu != json_password ){ jsonErr(F("no password")); return; }
      if ( mbRegIndex < 0 ){ jsonErr(F("bad addr")); return; }
      valu = jsonIn[F("valu")] | "none";
      if ( valu == "none" ){ jsonErr(F("no value")); return; }
      result = MBus_write_reg(mbReg, valu);
      if ( !result ) {
        jsonOut[F("writeSingleRegister")] = F("OK");
      } else {
        jsonErr("failure " + String(result)); return;
      }
      ok = true;

    } else if ( cmd == F("writeSingleCoil") ) {
      // write a coil  TODO
      #if DEBUG_ON>3
        debugMsg(F("json writeSingleCoil"));
      #endif
      String valu="";
      bool state;
      int result;
      String address = jsonIn[F("addr")] | "-1";
      int mbCoil = getDecInt(address);              // this covers all bases
      int mbCoilIndex = getCoilIndex(mbCoil);
      valu = jsonIn[F("pass")] | "none";
      if ( valu != json_password ){ jsonErr(F("no password")); return; }
      bool force = (jsonIn[F("force")] == "true") | false;
      if ( (mbCoilIndex < 0) && !force ){ jsonErr(F("bad addr")); return; }
      valu = jsonIn[F("valu")] | "none";
      if ( valu == "on" || valu == "true" ) {
        valu = F("on");
      } else if ( valu == "off" || valu == "false" ) {
        valu = F("off");
      } else {
        valu = F("none");
      }
      if ( valu == "none" ){ jsonErr(F("no value")); return; }
      result = MBus_write_coil(mbCoil, valu);
      if ( !result ) {
        jsonOut[F("writeSingleCoil")] = F("OK");
      } else {
        jsonErr("failure " + String(result)); return;
      }
      ok = true;
    } else if ( cmd == F("readRegRaw") ) {  
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
      int result;
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version;
      jsonOut[F("api_min")] = json_version_min;
      JsonArray &regArray = jsonOut.createNestedArray(F("registers"));
      String valu;
      uint16_t raw;
      // TODO count
      for (int i=0 ; i <10 ; i++) {
        JsonObject& regO      = regArray.createNestedObject();
        JsonObject& registers = regO.createNestedObject(F("register"));
        result = MBus_get_reg_raw(mbReg+i, raw);
        registers[F("addr")]   = mbReg+i;
        registers[F("vraw")]   = result ? F("err") : String("0x"+String(raw,HEX));
        registers[F("int")]    = result ? F("err") : String(raw);
        registers[F("f16")]    = result ? F("err") : String(IEEEf16::f32(raw),8);
      }
      ok = true;
    } else if ( cmd == F("getLogItem") ) {  
      String address = jsonIn[F("addr")] | "-1";
      int idx = getDecInt(address);              // this covers all bases (10, hex, and octal, specifically)
      long int lint;
      logItem item;
      int result;
      if ( idx < 0 || idx > 255 ) { jsonErr(F("bad addr")); return; };
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version;
      jsonOut[F("api_min")] = json_version_min;
      JsonArray &regArray = jsonOut.createNestedArray(F("log_items"));
            
      JsonObject& regO      = regArray.createNestedObject();
      JsonObject& registers = regO.createNestedObject(F("log_item"));
      if ( getLogItem(item, idx) ) { jsonErr(F("modbus error"), F("gateway timeout"), 504); return; }
      registers[F("addr")]              = idx;
      registers[F("hourmeter")]         = String(item.hourmeter);
      registers[F("alarm_daily")]       = "0x"+String(item.alarm_daily, HEX);
      if ( item.alarm_daily != 0 ) {   
        JsonObject& alarm      = registers.createNestedObject(F("alarms"));
        long int alarmbit = 1;
        int alarmNum = 1;
        for ( int i = 0 ;  i<32 ; i++ ) {
          if ( item.alarm_daily & alarmbit ) {
            #if DEBUG_ON>3
              debugMsg("alarmbit="+String(alarmbit,HEX));
              debugMsg("mbAlarmMsg-:" + String(i) + ":" + mbAlarmMsg[i]);
            #endif
            if ( mbAlarmMsg[i] != "n/a" && alarmNum <= 10 ) {   // skip unknowns, no more than 10 faults          
              alarm[String(alarmNum)] = mbAlarmMsg[i];
              alarmNum++;
            }
          }
          alarmbit = alarmbit << 1;
        }
      }
      registers[F("load_fault_daily")]  = "0x"+String(item.load_fault_daily, HEX);
      if ( item.load_fault_daily != 0 ) {  
        JsonObject& load      = registers.createNestedObject(F("load_faults"));
        long int alarmbit = 1;
        int alarmNum = 1;
        for ( int i = 0 ;  i<32 ; i++ ) {
          if ( item.load_fault_daily & alarmbit ) {
            #if DEBUG_ON>3
              debugMsg("i="+String(i));
              debugMsg("loadalarmbit="+String(alarmbit,HEX));
            #endif
            if ( mbLoadMsg[i] != "n/a" && alarmNum <= 10 ) {   // skip unknowns, no more than 10 faults
              load[String(alarmNum)] = mbLoadMsg[i];
              alarmNum++;
            }  
          }
          alarmbit = alarmbit << 1;
        }
      }
      registers[F("array_fault_daily")] = "0x"+String(item.array_fault_daily, HEX);
      if ( item.array_fault_daily != 0 ) {  
        JsonObject& array      = registers.createNestedObject(F("array_faults"));
        long int alarmbit = 1;
        int alarmNum = 1;
        for ( int i = 0 ;  i<32 ; i++ ) {
          if ( item.array_fault_daily & alarmbit ) {
            #if DEBUG_ON>3
              debugMsg("i="+String(i));
              debugMsg("array alarmbit="+String(alarmbit,HEX));
            #endif
            if ( mbArrayMsg[i] != "n/a" && alarmNum <= 10 ) {   // skip unknowns, no more than 10 faults   
              array[String(alarmNum)] = mbArrayMsg[i];
              alarmNum++;
            }
          }
          alarmbit = alarmbit << 1;
        }
      }
      registers[F("Vb_min_daily")]      = String(IEEEf16::f32(item.Vb_min_daily),8);      
      registers[F("Vb_max_daily")]      = String(IEEEf16::f32(item.Vb_max_daily),8);      
      registers[F("Ahc_daily")]         = String(IEEEf16::f32(item.Ahc_daily),8);      
      registers[F("Ahl_daily")]         = String(IEEEf16::f32(item.Ahl_daily),8);      
      registers[F("Va_max_daily")]      = String(IEEEf16::f32(item.Va_max_daily),8);      
      registers[F("time_ab_daily")]     = String(item.time_ab_daily);
      registers[F("time_eq_daily")]     = String(item.time_eq_daily);
      registers[F("time_fl_daily")]     = String(item.time_fl_daily);
      
      ok = true;
    }
    
    if (ok) {  
      String response_message;
      response_message.reserve(3000);
      response_message = jsonIn[F("back")] | "false";
      if ( response_message != "true" ) {
        response_message = "";
        jsonOut.prettyPrintTo(response_message);
        server.send(200, F("application/json"), response_message);
      } else {
        response_message = getHTMLHead();
        response_message += wrapScript(F("goBack()"));
        response_message += getHTMLFoot();
        server.send(200, F("text/html"), response_message);        
      }
    } else {
      jsonErr(F("general error"), F("server error"), 500);
    }
  } else {                                               // no json in request
  server.send(400, F("text/html"), F("400 - Bad request"));
  }
}
