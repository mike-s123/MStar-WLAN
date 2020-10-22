/* 
 *  This page handles rest requests, as documented in REST_API.txt
 *  (in FILESYSTEM).
 * 
 *  Requests and responses are in JSON.
 *  
 */

void jsonErr(AsyncWebServerRequest *request, String info, String message="bad request", int code=400 ) {
  String response_message;
  response_message.reserve(1000);
  response_message = F("{\"error\": {\"code\":");
  response_message += String(code);
  response_message += ",\"message\":\""+message+"\",\"info\":\""+info+"\"}}";
  request->send(code, F("application/json"), response_message); 
}

void restPageHandler(AsyncWebServerRequest *request) {                          //  URI /rest
/* example of use
 * http://192.168.168.151/rest?json={"addr":0,"cmd":"readHoldingRegisters","count":3}
 * err if the given address doesn't exist
 * will return the next "count" (default=1) available registers, 
 * skipping unavailable ones.
*/

  debugMsgln(F("Entering /rest page."),2);
  bool ok = false;
  if ( request->hasArg(F("json")) ) {  //Check if command received
    DynamicJsonDocument jsonIn(2000);
    DeserializationError jsonDesErr = deserializeJson(jsonIn, request->arg(F("json")).c_str() );
    String cmd = jsonIn[F("cmd")];
    String pass = jsonIn[F("pass")];
    #ifdef PS_RAM   // use PS-RAM if available
      struct SpiRamAllocator {
        void* allocate(size_t size) {
          return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        }
        void deallocate(void* pointer) {
          heap_caps_free(pointer);
        }
      };
      using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;
      SpiRamJsonDocument jsonOut(256000);
      debugMsgln("REST SPIRAM free heap: "+ String(ESP.getFreePsram()),3);
    #elif   
      DynamicJsonDocument jsonOut(65536);    // bigger is better
      debugMsgln("REST json free heap: "+String(ESP.getFreeHeap()),3);
    #endif
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
        { jsonErr(request, F("parsing failed"), F("parsing failed") , 413); return; }
      }
    } else if ( cmd == F("readHoldingRegisters") || cmd == F("readInputRegisters") || cmd == F("readInputRegister") ) {
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
      int count = jsonIn[F("count")] | 1;
      if ( cmd == "readInputRegister" ) { count = 1; };
      debugMsgln("REST read register(s) "+address+"/"+String(count),3);
      if ( count > 256 ) { jsonErr(request, F("count > 256"), F("request entity too large") , 413); return; }
      if ( getMbRegIndex(mbReg) < 0 ){ jsonErr(request, F("bad addr")); return; }
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version.c_str();
      jsonOut[F("api_min")] = json_version_min.c_str();
      JsonArray regArray = jsonOut.createNestedArray(F("registers"));
      for (int i=0; i<count && mbReg <= mbRegMax; mbReg++ ) {
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
      if ( count > 10 ) { jsonErr(request, F("count > 10"), F("request entity too large") , 413); return; }
      if ( getCoilIndex(mbCoil) < 0 ){ jsonErr(request, F("bad addr")); return; }
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version.c_str();
      jsonOut[F("api_min")] = json_version_min.c_str();
      JsonArray coilArray = jsonOut.createNestedArray(F("coils"));
      for (int i=0; i<count && mbCoil <= mbCoilMax; mbCoil++ ) {
        int coilIdx = getCoilIndex(mbCoil);
        bool state;
        if ( coilIdx >= 0 ) {
          JsonObject coilO  = coilArray.createNestedObject();
          JsonObject coils  = coilO.createNestedObject(F("coil"));
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
      debugMsgln(F("json writeSingleRegister"),4);
      String valu="";
      int result;
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
      int mbRegIndex = getMbRegIndex(mbReg);
      if ( !mbRegRW[mbRegIndex] ) { jsonErr(request, F("read only")); return; }
      valu = jsonIn[F("pass")] | "none";
      if ( valu != json_password.c_str() ){ jsonErr(request, F("no password")); return; }
      if ( mbRegIndex < 0 ){ jsonErr(request, F("bad addr")); return; }
      valu = jsonIn[F("valu")] | "none";
      if ( valu == "none" ){ jsonErr(request, F("no value")); return; }
      result = MBus_write_reg(mbReg, valu);
      if ( !result ) {
        jsonOut[F("writeSingleRegister")] = F("OK");
      } else {
        jsonErr(request, "failure " + String(result)); return;
      }
      ok = true;

    } else if ( cmd == F("writeSingleCoil") ) {
      // write a coil  TODO
      debugMsgln(F("json writeSingleCoil"),4);
      String valu="";
      bool state;
      int result;
      String address = jsonIn[F("addr")] | "-1";
      int mbCoil = getDecInt(address);              // this covers all bases
      int mbCoilIndex = getCoilIndex(mbCoil);
      valu = jsonIn[F("pass")] | "none";
      if ( valu != json_password.c_str() ){ jsonErr(request, F("no password")); return; }
      bool force = (jsonIn[F("force")] == "true") | false;
      if ( (mbCoilIndex < 0) && !force ){ jsonErr(request, F("bad addr")); return; }
      valu = jsonIn[F("valu")] | "none";
      if ( valu == "on" || valu == "true" ) {
        valu = F("on");
      } else if ( valu == "off" || valu == "false" ) {
        valu = F("off");
      } else {
        valu = F("none");
      }
      if ( valu == "none" ){ jsonErr(request, F("no value")); return; }
      result = MBus_write_coil(mbCoil, valu);
      if ( !result ) {
        jsonOut[F("writeSingleCoil")] = F("OK");
      } else {
        jsonErr(request, "failure " + String(result)); return;
      }
      ok = true;
    } else if ( cmd == F("readRegRaw") ) {  
      String address = jsonIn[F("addr")] | "-1";
      int mbReg = getDecInt(address);              // this covers all bases
      int result;
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version.c_str();
      jsonOut[F("api_min")] = json_version_min.c_str();
      JsonArray regArray = jsonOut.createNestedArray(F("registers"));
      String valu;
      uint16_t raw;
      int count  = jsonIn[F("count")] | 1;
      for (int i = 0 ; i < count ; i++) {
        JsonObject regO      = regArray.createNestedObject();
        JsonObject registers = regO.createNestedObject(F("register"));
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
      int count  = jsonIn[F("count")] | 1;
      if ( count > 32 ) { jsonErr(request, F("count > 256"), F("request entity too large") , 413); return; }
      if ( idx < 0 || idx > 255 ) { jsonErr(request, F("bad addr")); return; };
      jsonOut[F("model")] = model;
      jsonOut[F("api")] = json_version.c_str();
      jsonOut[F("api_min")] = json_version_min.c_str();
      JsonArray regArray = jsonOut.createNestedArray(F("log_items"));
      for (int i = 0 ; i < count && i < 256; i++) {
        JsonObject regO      = regArray.createNestedObject();
        JsonObject registers = regO.createNestedObject(F("log_item"));
        if ( getLogItem(item, idx+i) ) { jsonErr(request, F("modbus error"), F("gateway timeout"), 504); return; }
        registers[F("addr")]              = idx+i;
        registers[F("hourmeter")]         = String(item.hourmeter);
        registers[F("alarm_daily")]       = "0x"+String(item.alarm_daily, HEX);
        if ( item.alarm_daily != 0 ) {   
          JsonObject alarm      = registers.createNestedObject(F("alarms"));
          long int alarmbit = 1;
          int alarmNum = 1;
          for ( int i = 0 ;  i<32 ; i++ ) {
            if ( item.alarm_daily & alarmbit ) {
              debugMsgln("alarmbit="+String(alarmbit,HEX),4);
              debugMsgln("mbAlarmMsg-:" + String(i) + ":" + mbAlarmMsg[i],4);
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
          JsonObject load      = registers.createNestedObject(F("load_faults"));
          long int alarmbit = 1;
          int alarmNum = 1;
          for ( int i = 0 ;  i<32 ; i++ ) {
            if ( item.load_fault_daily & alarmbit ) {
              debugMsgln("i="+String(i),4);
              debugMsgln("loadalarmbit="+String(alarmbit,HEX),4);
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
          JsonObject array      = registers.createNestedObject(F("array_faults"));
          long int alarmbit = 1;
          int alarmNum = 1;
          for ( int i = 0 ;  i<32 ; i++ ) {
            if ( item.array_fault_daily & alarmbit ) {
               debugMsgln("i="+String(i),4);
               debugMsgln("array alarmbit="+String(alarmbit,HEX),4);
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
    }

    if (ok) {  
      String response_message;
      response_message.reserve(65536); //94000 
      debugMsgln("REST2 json free heap: "+String(ESP.getFreeHeap()),3);
      response_message = jsonIn[F("back")] | "false";
      if ( response_message != "true" ) {
        debugMsgln(F("Sending json response"),3);
        response_message = "";
        serializeJsonPretty(jsonOut,response_message);
        request->send(200, F("application/json"), response_message);
      } else {
        response_message = getHTMLHead();
        response_message += wrapScript(F("goBack()"));
        response_message += getHTMLFoot();
        request->send(200, F("text/html"), response_message);        
      }
    } else {
      jsonErr(request, F("general error"), F("server error"), 500);
    }
  } else {                                               // no json in request
  request->send(400, F("text/html"), F("400 - Bad request"));
  }
}
