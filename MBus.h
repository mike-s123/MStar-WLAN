/*   
    uint8_t  readCoils(uint16_t, uint16_t);
    uint8_t  readDiscreteInputs(uint16_t, uint16_t);
    uint8_t  readHoldingRegisters(uint16_t, uint16_t);
    uint8_t  readInputRegisters(uint16_t, uint8_t);
    uint8_t  writeSingleCoil(uint16_t, uint8_t);
    uint8_t  writeSingleRegister(uint16_t, uint16_t);
    uint8_t  writeMultipleCoils(uint16_t, uint16_t);
    uint8_t  writeMultipleCoils();
    uint8_t  writeMultipleRegisters(uint16_t, uint16_t);
    uint8_t  writeMultipleRegisters();
    uint8_t  maskWriteRegister(uint16_t, uint16_t, uint16_t);
    uint8_t  readWriteMultipleRegisters(uint16_t, uint16_t, uint16_t, uint16_t);
    uint8_t  readWriteMultipleRegisters(uint16_t, uint16_t);
*/

/*
 * static const uint8_t  ModbusMaster::ku8MBIllegalFunction = 0x01
  Modbus protocol illegal function exception. More...
 
static const uint8_t  ModbusMaster::ku8MBIllegalDataAddress = 0x02
  Modbus protocol illegal data address exception. More...
 
static const uint8_t  ModbusMaster::ku8MBIllegalDataValue = 0x03
  Modbus protocol illegal data value exception. More...
 
static const uint8_t  ModbusMaster::ku8MBSlaveDeviceFailure = 0x04
  Modbus protocol slave device failure exception. More...
 
static const uint8_t  ModbusMaster::ku8MBSuccess = 0x00
  ModbusMaster success. More...
 
static const uint8_t  ModbusMaster::ku8MBInvalidSlaveID = 0xE0
  ModbusMaster invalid response slave ID exception. More...
 
static const uint8_t  ModbusMaster::ku8MBInvalidFunction = 0xE1
  ModbusMaster invalid response function exception. More...
 
static const uint8_t  ModbusMaster::ku8MBResponseTimedOut = 0xE2
  ModbusMaster response timed out exception. More...
 
static const uint8_t  ModbusMaster::ku8MBInvalidCRC = 0xE3
  ModbusMaster invalid response CRC exception. More...
*/

String mbusErrLookup(uint8_t err){
  if (err == node.ku8MBIllegalFunction) return "ku8MBIllegalFunction";
  if (err == node.ku8MBIllegalDataAddress) return "ku8MBIllegalDataAddress";
  if (err == node.ku8MBIllegalDataValue) return "ku8MBIllegalDataValue";
  if (err == node.ku8MBSlaveDeviceFailure) return "ku8MBSlaveDeviceFailure";
  if (err == node.ku8MBSuccess) return "ku8MBSuccess";
  if (err == node.ku8MBInvalidSlaveID) return "ku8MBInvalidSlaveID";
  if (err == node.ku8MBInvalidFunction) return "ku8MBInvalidFunction";
  if (err == node.ku8MBResponseTimedOut) return "ku8MBResponseTimedOut";
  if (err == node.ku8MBInvalidCRC) return "ku8MBInvalidCRC";
  return "unknownError";
}

//-------------------------------------------------------------------
//------------------------- Modbus stuff ----------------------------
//-------------------------------------------------------------------

int mbRegAddr[REG_ROWS];
int mbRegMax;               // holds largest address found
String mbRegVar[REG_ROWS];
String mbRegDesc[REG_ROWS];
// TODO add V/A for ER_icomp
enum mbRegUnits {noUnit, Volt, Amp, Celcius, Ah, kWh, Watt, days, hours, seconds, percent};
String mbRegUnitName[] = {"-", "V", "A", "C", "Ah", "kWh", "W", "d", "h", "s", "%"};
mbRegUnits mbRegUnit[REG_ROWS];
bool mbRegRW[REG_ROWS];

int mbCoilAddr[COIL_ROWS];
int mbCoilMax;              // holds largest address found
String mbCoilVar[COIL_ROWS];
String mbCoilDesc[COIL_ROWS];
enum mbRegTypes {noType, f16, n10, dn10, sigint, usigint, dint, bitfield, dbitfield, bcd, n673, n1032, n1800, r417, n100, n7916, n9895, n96667, n13915, n66667, n31667, n9616, n1008, f32 };
String mbRegTypeName[] = {"none", "f16", "n10", "dn10", "int", "uint", "dint", "bitf", "dbitf", "bcd", "n673", "n1032", "n1800", "r417", "n100", "n79_16", "n989_5", "n96_667", "n139_15", "n66_667", "n316_67", "n96_16", "n100_8", "f32" }; 
mbRegTypes mbRegType[REG_ROWS];

String mbAlarmMsg[32];
String mbArrayMsg[32];
String mbLoadMsg[32];

struct fullReg { int row; String var; String desc; mbRegUnits unit; mbRegTypes type; String value; };

typedef uint16_t Float16;
struct logItem {
uint32_t hourmeter;
uint32_t alarm_daily;
uint32_t load_fault_daily;
uint32_t array_fault_daily;
Float16 Vb_min_daily;
Float16 Vb_max_daily;
Float16 Ahc_daily;
Float16 Ahl_daily;
Float16 Va_max_daily;
uint16_t time_ab_daily;
uint16_t time_eq_daily;
uint16_t time_fl_daily;
};

logItem mbLog[256];

int mbGetFullReg(fullReg &myReg, uint16_t address); // forward declaration

void rxEnable(bool state) {                                   // high = enabled = true
  state ? REG_WRITE(GPIO_OUT_W1TS_REG, 1 << RX_ENABLE_PIN) : REG_WRITE(GPIO_OUT_W1TC_REG, 1 << RX_ENABLE_PIN); // faster than digitalWrite
//  digitalWrite(RX_ENABLE_PIN, state);
}

void preTransmission() {                        // callbacks for half-duplex MODBUS
  rxEnable(false);                              // can't repeat requests too quickly, character time is
  delayMicroseconds(4011);                      // 11 bits/char @ 9600 = 0.00114583 s, *3.5 = Modbus char min interframe = 4.011 ms.
}

void postTransmission() {
//  uart_wait_tx_done(UART_NUM_1, 15);          // wait for uart 1 to empty, timeout is 3 RTOS ticks - ~150 ms, enough to empty full buffer (127)
                                                // doesn't work, results in "E (338308) uart: uart_wait_tx_done(1051): uart driver error"
  while (mbSerial.availableForWrite() < 0x7f) ; // delay until tx buffer is emptied (i.e. room for 0x7f chars)
  delayMicroseconds(1719);                      // 1.5 char wait (1719 us) for final char to clock out
                                                // 2005 (1.75),   4/130796 (0.003%) (1 index page)
                                                // 1719 (1.5), 6/219810 (0.003%) (1 index page running)
                                                // 1432 (1.25), lots of fails 64/66 (96.970%)
  rxEnable(true);
//  mbSerial.flush();                           // make sure we didn't receive our own stuff
  while (mbSerial.available()) mbSerial.read(); // because flush doesn't work?
}

int findAddrByVar(String var) {   // given var name, return addr of register, -1 if not found
  int addr = -1;
  bool done = false;
  for (int i = 1 ; (mbRegAddr[i] != mbRegMax) && !done ; i++) {
    if ( mbRegVar[i] == var ) {
      addr = mbRegAddr[i];
      done = true;
    }
  }
  debugMsgln("findAddrByVar="+var+", addr="+addr,7);
  return addr;  
}

int getMbRegIndex(int mbReg) {   // given a register address, returns the row index to look up info
  bool eof = false;
  for ( int row = 1; row <= REG_ROWS && !eof ; row++ ) {
    if (mbRegAddr[row] < 0) { eof = true;
    } else if ( mbReg==mbRegAddr[row] ) {
      return row;
    }
  }
  return -1;
}

void getDescVal(uint16_t address, String &desc, String &val) {
  fullReg reg;
  reg.desc = F("err");
  mbGetFullReg(reg, address);
  desc = reg.desc;
  val = reg.value + " " + mbRegUnitName[reg.unit];
  debugMsgln("getDescVal desc="+desc+", val="+val,7);
}

int getCoilIndex(int coil) {     // similar to getMbRegIndex
  bool eof = false;
  for ( int row = 1; row <= COIL_ROWS && !eof ; row++ ) {
    if (mbCoilAddr[row] < 0) { eof = true;
    } else if ( coil==mbCoilAddr[row] ) {
      return row;
    }
  }
  return -1;
}

int MBus_get_coil(uint16_t address, bool &value) {             // returns 0 on success
  if (noController) return -1;
  debugMsgln("MBus_get_coil: "+String(address),7);
  uint8_t result = 1;
  for ( int i = 0 ; (i < 3) && result ; i++ ) {           // try up to 3 times
    result = node.readCoils(address, 1);                   // succcess = 0
    mbustries++;
    if (result) {
      mbuserrs++;
      debugMsgln(F("MBus_get_coil, mbuserr."),2);
    }
    delay(i*50);                                           // delay a bit more on each attempt
  }
  if (result == node.ku8MBSuccess)  {
    value = (node.getResponseBuffer(0))?true:false; 
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}

int MBus_write_coil(uint16_t address, String valu) {           // returns 0 on success
  if (noController) return -1;
  bool state;
  valu == "on" ? state=true : state=false ;
  debugMsgln("MBus_write_coil: "+String(address)+", "+String(state),7);
  int row;
  int result;
  row = getCoilIndex(address);
  if ( row > 0 ) {
    result = node.writeSingleCoil(address, state);
    mbustries++;
    if (result) {
      mbuserrs++;
      debugMsgln(F("MBus_write_coil, mbuserr."),2);
    }
    if ( ( address == 255 || address == 254 ) && state && result == node.ku8MBResponseTimedOut ){  
      result = 0;  // we reset the controller, timeout expected
    }
    debugMsgln("Result: "+String(result),7);
    return result; 
  } else {
    return -1;
  }
}

int MBus_get_reg_raw(uint16_t address, uint16_t &raw) {    // get register uninterpreted
  if (noController) return -1;
  int result=1;
  debugMsgln("MBus_get_reg_raw: "+String(address),4);
  int newMbusErr = 0;
  
  for ( int i = 0 ; (i < 3) && (result != node.ku8MBSuccess) ; i++ ) {  // try 3 times
    mbustries++;
    result = node.readHoldingRegisters(address, 1);
    if (result == node.ku8MBSuccess) {
      raw = node.getResponseBuffer(0);
    } else {
      delay((i*50)+25); // increasing delay between failed attempts
      mbuserrs++;
      newMbusErr++;
      debugMsgln("MBus_get_reg_raw, mbuserr:"+mbusErrLookup(result)+" addr:"+String(address),2);
    }
  }
  if (newMbusErr > 0 && result == node.ku8MBSuccess) {
    debugMsgln("Recovered from error.",2);
    mbuserrs_recovered++;
  }

/*  
  result = node.readHoldingRegisters(address, 1);
  if (result == node.ku8MBSuccess) {
      raw = node.getResponseBuffer(0);
    };
*/    
  return result;
}

int MBus_get_int(uint16_t address, int &value) {                 // get signed int
  if (noController) return -1;
  debugMsgln("MBus_get_int: "+String(address),7);
  uint16_t reg;
  int result = MBus_get_reg_raw(address, reg);
  if (result == node.ku8MBSuccess)  {
    value = reinterpret_cast<int16_t&>(reg);             // change unsigned to signed
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}

int MBus_get_uint16(uint16_t address, uint16_t &value) {         // get unsigned int
  if (noController) return -1;
  debugMsgln("MBus_get_uint16: "+String(address),7);
  uint16_t reg;
  int result = MBus_get_reg_raw(address, reg);
  if (result == node.ku8MBSuccess)  {
    value = reg ;
  }
  debugMsgln("Got "+String(value),7);
  debugMsgln("Result: "+String(result),7);
  return result;
}

int MBus_get_n10(uint16_t address, float &value) {             // get 10x unsigned int, return float
  if (noController) return -1;
  debugMsgln("MBus_get_n10: "+String(address),7);
  uint16_t intval;
  int result = MBus_get_uint16(address, intval);
  if (result == node.ku8MBSuccess) {
    value = intval/10.;
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}
  
int MBus_get_uint32(uint16_t address, uint32_t &value) {       // get unsigned long int, high first
  if (noController) return -1;
  debugMsgln("MBus_get_uint32: "+String(address),7);
  int result = node.readHoldingRegisters(address, 2);
  mbustries++;
  if (result == node.ku8MBSuccess) {
    value = node.getResponseBuffer(0) << 16 ;         // HI first
    value += node.getResponseBuffer(1) ;
  } else {
    mbuserrs++;
    debugMsgln(F("MBus_get_uint32, mbuserr."),2);
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}
  
int MBus_get_uint32_rev(uint16_t address, uint32_t &value) {  // get unsigned long int, low first
  if (noController) return -1;
  debugMsgln("MBus_get_uint32: "+String(address),7);
  int result = node.readHoldingRegisters(address, 7);
  mbustries++;
  if (result == node.ku8MBSuccess) {
    value = node.getResponseBuffer(1) << 16 ;         // LO first
    value += node.getResponseBuffer(0) ;
  } else {
    mbuserrs++;
    debugMsgln(F("MBus_get_uint32_rev, mbuserr."),2);
  }
  debugMsgln("Result: "+String(result),4);
  return result;
}

int MBus_get_dn10(uint16_t address, float &value) {        // get long 10x, return float
  if (noController) return -1;
  debugMsgln("MBus_get_dn10: "+String(address),7);
  uint32_t intval;
  int result = MBus_get_uint32(address, intval);
  if (result == node.ku8MBSuccess) {
    value = intval/10.;
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}  

int MBus_get_float(uint16_t address, float &value) {     // get float16, return float32
  if (noController) return -1;
  debugMsgln("MBus_get_float: "+String(address),7);
  uint16_t reg;
  int result = MBus_get_reg_raw(address, reg);
  if (result == node.ku8MBSuccess) {
    value = IEEEf16::f32(reg);
  }
  debugMsgln("Result: "+String(result),7);
  return result;
}

int MBus_get_reg(uint16_t address, String &value) {         // given an address, looks up type and gets value. returns 0 on success
  if (noController) return -1;
  debugMsgln("MBus_get_reg: "+String(address),7);
  int row, result = 1, foo_sint;
  uint16_t foo_int;
  float foo_fl;
  uint32_t foo_dint;
  
  row = getMbRegIndex(address);
  if ( row ) {
    for ( int i = 0 ; (i < 3) && result ; i ++ ) {                          // try up to 3 times
      switch (mbRegType[row]) {
        case f16:     result = MBus_get_float(mbRegAddr[row], foo_fl);
                      result ? value = F("err") : value = String(foo_fl);
                      break;
        case n10:     result = MBus_get_n10(mbRegAddr[row], foo_fl);
                      result ? value = F("err") : value = String(foo_fl);        
                      break;
        case dn10:    result = MBus_get_dn10(mbRegAddr[row], foo_fl);
                      result ? value = F("err") : value = String(foo_fl);        
                      break;
        case sigint:  result = MBus_get_int(mbRegAddr[row], foo_sint);
                      result ? value = F("err") : value = String(foo_sint);                
                      break;
        case usigint: result = MBus_get_uint16(mbRegAddr[row], foo_int);
                      result ? value = F("err") : value = String(foo_int);        
                      break;
        case dint:    result = MBus_get_uint32(mbRegAddr[row], foo_dint);
                      result ? value = F("err") : value = String(foo_dint);        
                      break;
        case bitfield: result = MBus_get_uint16(mbRegAddr[row], foo_int);
                      result ? value = F("err") : value = String(foo_int);        
                      break;
        case dbitfield: result = MBus_get_uint32(mbRegAddr[row], foo_dint);
                      result ? value = F("err") : value = String(foo_dint);        
                      break;
        case bcd:     result = MBus_get_uint16(mbRegAddr[row], foo_int);
                      result ? value = F("err") : value = String(foo_int);        
                      break;
        case n673:    result = MBus_get_uint16(mbRegAddr[row], foo_int); // SSDuo datatype, n/673 
                      result ? value = F("err") : value = String(foo_int/673.0);        
                      break;
        case n1032:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // SSDuo datatype, n/1032
                      result ? value = F("err") : value = String(foo_int/1032.0);        
                      break;
        case n1800:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // SSDuo datatype, n/1800
                      result ? value = F("err") : value = String(foo_int/1800.0);        
                      break;
        case r417:    result = MBus_get_uint16(mbRegAddr[row], foo_int); // SSDuo datatype, % 0-417 ??? TODO:test result
                      result ? value = F("err") : value = String(foo_int/4.17);        
                      break;
        case n100:    result = MBus_get_uint16(mbRegAddr[row], foo_int); // SS-MPPT datatype, n·100·2^-15
                      result ? value = F("err") : value = String(foo_int*(100/32768));        
                      break;
        case n7916:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // SS-MPPT datatype, n·79.16·2^-15
                      result ? value = F("err") : value = String(foo_int*(79.16/32768));        
                      break;
        case n9895:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // SS-MPPT datatype, n·989.5·2^-16
                      result ? value = F("err") : value = String(foo_int*(989.5/65536));        
                      break;
        case n96667:  result = MBus_get_uint16(mbRegAddr[row], foo_int); // SS-MPPT datatype, n·96.667^2-15
                      result ? value = F("err") : value = String(foo_int*(99.667/32768));        
                      break;
        case n13915:  result = MBus_get_uint16(mbRegAddr[row], foo_int); // TS datatype, n·139.15·2^-15
                      result ? value = F("err") : value = String(foo_int*(139.15/32768));        
                      break;
        case n66667:  result = MBus_get_uint16(mbRegAddr[row], foo_int); // TS datatype, n·66.667·2^-15
                      result ? value = F("err") : value = String(foo_int*(66.667/32768));        
                      break;
        case n31667:  result = MBus_get_uint16(mbRegAddr[row], foo_int); // TS datatype, n·316.67·2^-15
                      result ? value = F("err") : value = String(foo_int*(316.67/32768));        
                      break;
        case n9616:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // TS datatype, n·96.667·2-16
                      result ? value = F("err") : value = String(foo_int*(96.667/65536));        
                      break;
        case n1008:   result = MBus_get_uint16(mbRegAddr[row], foo_int); // SS-MPPT datatype, n*100*2^-8
                      result ? value = F("err") : value = String(foo_int*(100.0/256.0));        
                      break;
        // TODO f32, TS-600 datatype
        default:           ;
      }
      if (result) delay(i*50); // increasing delay between failed attempts 
    }  // try 3 times
    return result;  // false (0) if success
  } else {
    return -1;   
  } 
}

int mbGetFullReg(fullReg &myReg, uint16_t address) {  //given address, gets all we know about it
  if (noController) {
    myReg.row = 0; 
    myReg.var = "";
    myReg.desc = "";
    myReg.unit = noUnit;
    myReg.type = noType;
    myReg.value = "";
    return -1;
  }
  debugMsgln("MBus_get_fullreg: "+String(address),7);
  uint16_t foo_int;
  int foo_sint, result = 0, row;
  float foo_fl;
  uint32_t foo_dint;
  if ( address < 0 ) { return -1; }
  row = getMbRegIndex(address);
  if ( row <= 0 ) { return -1; }
  myReg.row = row; 
  myReg.var = mbRegVar[myReg.row];
  myReg.desc = mbRegDesc[myReg.row];
  myReg.unit = mbRegUnit[myReg.row];
  myReg.type = mbRegType[myReg.row];
  if ( myReg.row ) {
    result = MBus_get_reg(address, myReg.value);
  } 
  return result;  
}

int MBusWSR(uint16_t address, unsigned int value) {
  int result = node.writeSingleRegister(address, value);
  mbustries++;
  if (result) {
    mbuserrs++;
    debugMsgln(F("MBusWSR, mbuserr."),2);
  }
  return result;
}


int MBus_write_reg(uint16_t address, String valu) {
  if (noController) return -1;
  debugMsgln("MBus_write_reg: "+String(address)+", "+String(valu),4);
  int row;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  uint16_t value1;
  uint16_t value2;
  int result = -1; // failure unless we say otherwise
    
  row = getMbRegIndex(address);
  debugMsgln("type "+String(mbRegTypeName[mbRegType[row]]),5);
  switch (mbRegType[row]) {
    case f16:     value1 = IEEEf16::f16(valu.toFloat());
                  foo_fl = IEEEf16::f32(value1);
                  debugMsgln("writing 0x"+String(value1, HEX)+", "+String(foo_fl),5);
                  result = MBusWSR(address, value1);
                  break;
    case n10:     value1 = valu.toFloat()*10;
                  debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/10",5);
                  result = MBusWSR(address, value1);
                  break;
    case dn10:    break;    // TODO ??
    case sigint:  ;
    case usigint: value1 = valu.toInt();
                  debugMsgln("Writing 0x"+String(value1, HEX)+", "+String(value1),5);
                  result = MBusWSR(address, value1);
                  break;
    case dint:    break;   // TODO ??
    case bitfield: break;
    case dbitfield: break;
    case bcd:     break;
// TODO datatypes below are untested    
    case n673:  value1 = static_cast<int>(valu.toFloat() * 673); // SSDuo datatype, n/673 
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/673",5);
                result = MBusWSR(address, value1);
                break;
    case n1032: value1 = static_cast<int>(valu.toFloat() * 1032); // SSDuo datatype, n/1032
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/1032",5);
                result = MBusWSR(address, value1);
                break;
    case n1800: value1 = static_cast<int>(valu.toFloat() * 1800); // SSDuo datatype, n/1800
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/1800",5);
                result = MBusWSR(address, value1);
                break;
    case r417:  break; // SSDuo datatype, % 0-417 ??? TODO:test result
    case n100:  value1 = static_cast<int>(valu.toFloat() * (32768/100)); // SS-MPPT datatype, n·100·2^-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/100)",5);
                result = MBusWSR(address, value1);
                break;
    case n7916: value1 = static_cast<int>(valu.toFloat() * (32768/79.16)); // SS-MPPT datatype, n·79.16·2^-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/79.16)",5);
                result = MBusWSR(address, value1);
                break;
    case n9895: value1 = static_cast<int>(valu.toFloat() * (65536/989.5)); // SS-MPPT datatype, n·989.5·2^-16
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(65536/989.5)",5);
                result = MBusWSR(address, value1);
                break;
    case n96667: value1 = static_cast<int>(valu.toFloat() * (32768/96.667)); // SS-MPPT datatype, n·96.667^2-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/96.667)",5);
                result = MBusWSR(address, value1);
                break;
    case n13915: value1 = static_cast<int>(valu.toFloat() * (32768/139.15)); // TS datatype, n·139.15·2^-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/139.15)",5);
                result = MBusWSR(address, value1);
                break;
    case n66667: value1 = static_cast<int>(valu.toFloat() * (32768/66.667)); // TS datatype, n·66.667·2^-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/66.667)",5);
                result = MBusWSR(address, value1);
                break;
    case n31667: value1 = static_cast<int>(valu.toFloat() * (32768/316.67)); // TS datatype, n·316.67·2^-15
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/316.67)",5);
                result = MBusWSR(address, value1);
                break;
    case n9616: value1 = static_cast<int>(valu.toFloat() * (65536/96.667)); // TS datatype, n·96.667·2-16
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(65536/96.667)",5);
                result = MBusWSR(address, value1);
                break;
    case n1008: value1 = static_cast<int>(valu.toFloat() * (256.0/100.0)); // SS-MPPT datatype, n*100*2^-8
                debugMsgln("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(256.0/100.0)",5);
                result = MBusWSR(address, value1);
                break;
      // TODO f32             
    default:           ;
  }
  debugMsgln("Result: "+String(result),5);
  return result; 
}

int MBus_get_regs_raw(uint16_t address, uint16_t *rawarray, int count) {  //TODO this returns more than requested.
  if (noController) return -1;
  debugMsgln("MBus_get_regs_raw: "+String(count)+"x, "+String(address),7);
  int result = 0;
  int passes = count / 32; // modbusmaster buffer is 64 bytes
  int leftover = count %32;
  debugMsgln("RegsRaw, passes "+String(passes)+", left:"+String(leftover),7);
  int i;
  for (i = 0; i < passes; i++) {
//    delay(5);
    result = node.readHoldingRegisters(address+(i*32), 32);
    mbustries++;
    if (result) {
      mbuserrs++;
      debugMsgln(F("MBus_get_regs_raw1, mbuserr."),2);
    }
    for (int j = 0; j<32; j++) {
        rawarray[(i*32)+j] = node.getResponseBuffer(j);
        debugMsgln("rawarray "+String((i*32)+j)+"="+String(node.getResponseBuffer(j),HEX),8);
    }
  }
//  delay(5);
  result += node.readHoldingRegisters(address+i*32, leftover);
  mbustries++;
  if (result) {
    mbuserrs++;
    debugMsgln(F("MBus_get_regs_raw2, mbuserr."),2);
  }
  for ( i=0; i<leftover; i++) {
    rawarray[passes*32+i] = node.getResponseBuffer(i);
  }
  return result;
}

int getLogItem(logItem &item, int idx) {   // idx starts at 0, returns 0 on success
  if (noController) return -1;
  uint16_t startReg = 32768 + (idx * 16);
  int result = MBus_get_uint32_rev(startReg, item.hourmeter);
  debugMsg(F("getLogItem, startReg=0x"),4);
  debugMsgln(String(startReg,HEX),4);
  debugMsg(F("getLogItem, hourmeter="),4);
  debugMsgln(String(item.hourmeter),4); 
  delayMicroseconds(1000);
  result += MBus_get_uint32_rev(startReg+2, item.alarm_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint32_rev(startReg+4, item.load_fault_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint32_rev(startReg+6, item.array_fault_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+8, item.Vb_min_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+9, item.Vb_max_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+10, item.Ahc_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+11, item.Ahl_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+12, item.Va_max_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+13, item.time_ab_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+14, item.time_eq_daily);
  delayMicroseconds(1000);
  result += MBus_get_uint16(startReg+15, item.time_fl_daily);
  return result;
}

int getLog() {                          // get all of log into mbLog (32K)
  int result = 0;
  for (int i=0 ; i< 256 ; i++ ) {
    result += getLogItem(mbLog[i],i);
  }
  return result;
}

    /*
     *  MODBUS read device ID (function 43 / 14)
     *  Modbus-master doesn't support this, so we do it here,
     *  just enough to support what MSView wants - the
     *  mandatory "BASIC" category elements.
     */

int readDeviceID(String &vendorName, String &productCode, String &majorMinorRevision) {
  /*
   * Begin with partially pre-formed query to send.
   * MODBUS address
   * 0x2b 0x0e read device id.
   * 0x01 request to get basic device identification (VendorName, ProductCode, MajorMinorRevision) (stream access)
   * 0x00 start with object 0 (VendorName)
   * last 2 bytes hold CRC
   */
  unsigned char query[] = {(uint8_t)mbAddr, 0x2b, 0x0e, 0x01, 0x00, 0x00, 0x00};
  char vendor[32], product[32], revision[32], response[128];
  uint8_t count, id, id_idx, numObjs;
  uint16_t crc = 0xffff;
  for (int i=0 ; i < 5 ; i++) {
    crc = crc16_update(crc, query[i]);
  }
  query[5] = (uint8_t)(crc & 0x00FF);
  query[6] = (uint8_t)((crc & 0xFF00) >> 8);  // add the CRC to the query
  debugMsgln("Mbus CRC out = " + String(crc,HEX),7);

  bool goodStart;
  for (int tries=1; tries < 3; tries++) {
    goodStart=false;
    preTransmission();
    mbSerial.flush(); // clear buffers
    mbSerial.write(query, 7);
    postTransmission();
    
    int startwait = 300; // up to 300 ms for start of a response.
    int charwait = 2;   // no more than 1.5 char times allowed, should come ~1.1 ms apart
    // loop until we run out of time or bytes, or an error occurs
    uint32_t doneTime = millis() + startwait; 
    bool done = false;
    count = 0;
    
    while ( !done ) {
      if (mbSerial.available()) {
        int uartchars = mbSerial.available();
        char incoming = mbSerial.read();
        debugMsg(String(F("readDeviceID, mbSerial avail, count: ")) + String(uartchars,DEC),7); 
        debugMsgln(String(F(", char:")) + String(incoming, DEC),7);
        if (incoming == mbAddr) goodStart = true; // first byte of response should be address
        if (goodStart) response[count++] = incoming;
        doneTime = millis() + charwait; // started getting a response, change to char timeout
      }  //matches xx.available
      if (millis() > doneTime) { done = true; }  // no response or didn't get a char for a while
    } // while() - done getting response
    mbustries++;
    if (goodStart) {
      tries=4;
    } else {
      mbuserrs++;
      debugMsgln(F("readDeviceID, mbuserr."),2);
      delay(tries*50); // delay a bit more between each try
    }
  }
  debugMsgln(String(F("readDeviceID, bytes read:")) + String(count),5);
  
  if (count <= 1) { return -1; } else {count = 0;}
  id_idx = 8;
  numObjs = response[id_idx-1];
  vendorName = "";
  productCode = "";
  majorMinorRevision = "";
  for (int i = 0; i < numObjs ; i++) {
    id = response[id_idx]; // which object
    count = response[id_idx+1]; // length
    debugMsgln("readDeviceID, id="+String(id),5);
    for (int j = 0 ; j < count; j++) {
      switch (i) {
        case 0: 
                vendorName += response[id_idx+2+j];
                break;
        case 1: 
                productCode += response[id_idx+2+j];
                break;
        case 2: 
                majorMinorRevision += response[id_idx+2+j];
                break;
      }
    }
    id_idx = id_idx + count + 2;
  }
  
  debugMsg(F("readDeviceID, vendorName="),5);
  debugMsgln(vendorName,5);
  debugMsg(F("readDeviceID, productCode="),5);
  debugMsgln(productCode,5);
  debugMsg(F("readDeviceID, majorMinorRevision="),5);
  debugMsgln(majorMinorRevision,5);

/*  //example
  vendorName = String("Morningstar Corp.");
  productCode = String("PS-PWM-30M");
  majorMinorRevision = String("v01.04.06");
*/
  return 0;
}

int mbusTCP() {
  if (noController) return -1;
  uint8_t mbbuffer[1024];
  uint8_t result, func, mei, readID, objID; 
  uint16_t refNum, count, len, val, buffsize;
  /*
   * If something is actively using this, we're not
   * going to let wifi do scanning.
   */
  if ( (millis() - lastWLANtry) < 5000 ) lastWLANtry += 5000;  // at least 5 seconds
  if (modbusClient.available() > 0) {
    int i;
    for ( i = 0; i < 6; i++) {
      mbbuffer[i] = modbusClient.read();
    }
    len = mbbuffer[4]*256 + mbbuffer[5];
    for ( i = 0 ; i < len ; i++ ) {
      mbbuffer[i+6] = modbusClient.read();
    }
    func = mbbuffer[7];
    buffsize = 6 + len;
    debugMsg(F("modbusTCP received"),7);
    for (i=0;i<6+len;i++){
      debugMsg("."+String(mbbuffer[i],HEX),7);
    }
    debugMsgln("",7);
    
    result = 255;
    if (func == 3 || func == 4) {  //read holding/inputs
      result = 0;
      refNum = mbbuffer[8]*256 + mbbuffer[9];
      count = mbbuffer[10]*256 + mbbuffer[11];
      buffsize = 9;
      uint16_t ar[256];
      
      result += MBus_get_regs_raw(refNum, ar, count);
      for (i=0; i<count; i++) {
        mbbuffer[buffsize+(2*i)]   = (uint8_t)((ar[i] & 0xFF00) >> 8);
        mbbuffer[buffsize+(2*i)+1] = (uint8_t)(ar[i] & 0x00FF);
      }
      buffsize = buffsize + 2*count;

      mbbuffer[4] = (uint8_t)((buffsize-6 & 0xFF00) >> 8);
      mbbuffer[5] = (uint8_t)(buffsize-6 & 0x00FF);   //length
      mbbuffer[8] = 2*count;  //byte count

    } else if (func == 5) { //write single coil
      bool state;
      result = 0;
      refNum = mbbuffer[8]*256 + mbbuffer[9];   //register
      mbbuffer[10] == 0xff ? state = true : state = false ;
      result = node.writeSingleCoil(refNum, state);
      mbustries++;
      if (result) {
        mbuserrs++;
        debugMsgln(F("mbusTCP5, mbuserr."),2);
      }
      // good response is to just return what we got...

    } else if (func == 6) { //write single register
      result = 0;
      refNum = mbbuffer[8]*256 + mbbuffer[9];   //register
      val    = mbbuffer[10]*256 + mbbuffer[11]; //value
      result = node.writeSingleRegister(refNum, val);
      mbustries++;
      if (result) {
        mbuserrs++;
        debugMsgln(F("mbusTCP6, mbuserr."),2);
      }
      // good response is to just return what we got...

    } else if (func == 43) { // Read device ID, 
      String vendorName, productCode, majorMinorRevision;
      vendorName.reserve(64);
      productCode.reserve(64);
      majorMinorRevision.reserve(64);
      result = readDeviceID(vendorName, productCode, majorMinorRevision);
      debugMsg(F("MbusTCP, vendorName="),5);
      debugMsgln(vendorName,5);
      debugMsg(F("MbusTCP, productCode="),5);
      debugMsgln(productCode,5);
      debugMsg(F("MbusTCP, majorMinorRevision="),5);
      debugMsgln(majorMinorRevision,5);

      mei = mbbuffer[8];
      readID =  mbbuffer[9];
      objID = mbbuffer[10];
      char response[128];
      
      strcpy(response, vendorName.c_str());
      mbbuffer[10]=1; // conformity level
      mbbuffer[11]=0xff; // more (doesn't) follow
      mbbuffer[12]=1; // next object id
      mbbuffer[13]=3; // number of objects
      mbbuffer[14]=0; // object id
      mbbuffer[15]=strlen(response); // object length
      for (i = 0 ; i < strlen(response); i++ ) {
        mbbuffer[i+16]= (uint8_t)response[i]; 
      }
      buffsize = 16 + strlen(response);
      
      strcpy(response, productCode.c_str());
      for (i = 0 ; i < strlen(response); i++ ) {
        mbbuffer[buffsize+2+i]= (uint8_t)response[i]; 
      }
      mbbuffer[buffsize] = 1; //object id
      mbbuffer[buffsize+1] = (uint8_t)strlen(response); //length
      buffsize = buffsize + 2 + strlen(response);

      strcpy(response, majorMinorRevision.c_str());
      for (i = 0 ; i < strlen(response); i++ ) {
        mbbuffer[i+buffsize+2]= (uint8_t)response[i]; 
      }
      mbbuffer[buffsize] = 2; //object id
      mbbuffer[buffsize+1] = (uint8_t)strlen(response); //length
      buffsize = buffsize + 2 + strlen(response);               
      val = buffsize-6;
      mbbuffer[4] = (uint8_t)((val & 0xFF00) >> 8);
      mbbuffer[5] = (uint8_t)(val & 0x00FF);
    }
    if (!result) {
      debugMsg(F("modbusTCP sending"),7);
      for (i=0;i<buffsize;i++){
        debugMsg("."+String(mbbuffer[i],HEX),7);
      }
      debugMsgln("",7);
      modbusClient.write((const uint8_t *)mbbuffer, buffsize);
    } else {
      ; // TODO send error response
    }
  }
}

String getModel() {
  String vendorName = "", productCode = "", majorMinorRevision = "", mod = "";
  int result;
  vendorName.reserve(64);
  productCode.reserve(64);
  majorMinorRevision.reserve(64);
  result = readDeviceID(vendorName, productCode, majorMinorRevision);
  if (result == -1 || productCode.length() > 20) productCode = "";
  debugMsg(F("getModel, readDeviceID returned:"),5);
  debugMsgln(String(result),5);
  
  /* Model names
   * PS-PWM*, PS-MPPT*, SSDuo (SunSaver Duo), SS-MPPT*, TS-45 or TS-60 (Tristar),
   * TS-MPPT-60-600V-48, TS-MPPT-30, TS-MPPT-45, TS-MPPT-60,
   */
  if (productCode.startsWith(F("PS-PWM"))) {            mod = "PS-PWM"; }
  if (productCode.startsWith(F("PS-MPPT"))) {           mod = "PS-MPPT"; }
  if (productCode.startsWith(F("SS-MPPT"))) {           mod = "SS-MPPT"; }
  if (productCode.startsWith(F("SSDuo"))) {             mod = "SSDuo"; }
  if (productCode.startsWith(F("TS-"))) {               mod = "TS"; }
  if (productCode.startsWith(F("TS-MPPT")))  {          mod = "TS-MPPT"; }
  if (productCode.startsWith(F("TS-MPPT-60-600V"))) {   mod = "TS-600"; }
  debugMsg(F("getModel received:"),4);
  debugMsgln(productCode,4);
  debugMsg(F("getModel selected:"),4);
  debugMsgln(mod,4);
  fullModel = productCode;
  return mod;
}

void checkController() {
  String lastModel = model;
  bool lastState = noController;
  if (noController || lastFound + 300000 < millis()) {  // check if no controller, or it's been more than 5 minutes
    lastFound = millis();
    model = getModel();
    if (model == "") {
      delay(1000);
      model = getModel(); // try again if not found
    }
    if (model == "") {
      if (!lastState) debugMsgln(F("No controller found."),1);
      noController = true;
    } else {
      if (lastState) {
        debugMsg(F("Controller found:"),1);
        debugMsgln(model,1);
        noController = false;
        refreshCtlLogFile();
      }
      if (lastModel != model) {             // model changed
        void getFile(String model); // fwd declaration
        getFile(model);
        refreshCtlLogFile();
      }
    }
  }
}

bool controllerNeedsReset() {
  bool reset = false;
  uint16_t raw=0;
  int reg=0;
  int i;
  reg = findAddrByVar(F("load_fault"));
  MBus_get_reg_raw(reg, raw);
  for (i=0 ; i<16 ; i++) {
    if ( mbLoadMsg[i].startsWith(F("EEPROM")) ) {
      if ( raw & (0x0001 << i) ) reset = true;
    }
  }
  
  reg = findAddrByVar(F("array_fault"));
  MBus_get_reg_raw(reg, raw);
  for (i=0 ; i<16 ; i++) {
    if ( mbArrayMsg[i].startsWith(F("EEPROM")) ) {
      if ( raw & (0x0001 << i) ) reset = true;
    }
  }

  if (reset) debugMsgln(F("Controller needs reset"),2);
  return reset;
}
