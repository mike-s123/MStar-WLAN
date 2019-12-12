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
//String mbErrors[] = {F("Illegal Function"), F("Illegal Address"), F("Illegal Value"), F("Server Failure"), F("Ack"), F("Server Busy")};

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

int mbGetFullReg(fullReg &myReg, int address); // forward declaration

void rxEnable(bool state) {                                   // high = enabled = true
  #ifdef ARDUINO_ARCH_ESP8266
    state ? GPOS = 1 << RX_ENABLE_PIN : GPOC = 1 << RX_ENABLE_PIN;  // faster than digitalWrite
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    state ? REG_WRITE(GPIO_OUT_W1TS_REG, 1 << RX_ENABLE_PIN) : REG_WRITE(GPIO_OUT_W1TC_REG, 1 << RX_ENABLE_PIN); // faster than digitalWrite
  #endif
//  digitalWrite(RX_ENABLE_PIN, state);
}
void preTransmission() {
  rxEnable(false);
  delay(5);               // can't repeat requests too quickly, minimum interframe 3.65 ms
}
void postTransmission() {
  rxEnable(true);
  delay(2);               // just to give background stuff a chance to run
}

int findAddrByVar(String var) {   // given var name, return addr of register, -1 if not found
  int addr = -1;
  for (int i = 1 ; mbRegAddr[i] != mbRegMax ; i++) {
    if ( mbRegVar[i] == var ) {
      addr = mbRegAddr[i]; 
    }
  }
  #if DEBUG_ON>3
    debugMsg("findAddrByVar="+var+", addr="+addr);
  #endif  
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

void getDescVal(int address, String &desc, String &val) {
  fullReg reg;
  reg.desc = F("err");
  mbGetFullReg(reg, address);
  desc = reg.desc;
  val = reg.value + " " + mbRegUnitName[reg.unit];
  #if DEBUG_ON>3
    debugMsg("getDescVal desc="+desc+", val="+val);
  #endif  
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

int MBus_get_coil(int address, bool &value) {             // returns 0 on success
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_coil: "+String(address));
  #endif
  uint8_t result = 1;
  for ( int i = 0 ; (i < 3) && result ; i ++ ) {           // try up to 3 times
    result = node.readCoils(address, 1);                   // succcess = 0
    delay(i*50);
  }
  if (result == node.ku8MBSuccess)  {
    value = (node.getResponseBuffer(0))?true:false; 
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

int MBus_write_coil(int address, String valu) {           // returns 0 on success
  if (noController) return -1;
  bool state;
  valu == "on" ? state=true : state=false ;
  #if DEBUG_ON>3
    debugMsg("MBus_write_coil: "+String(address)+", "+String(state));
  #endif
  int row;
  int result;
  row = getCoilIndex(address);
  if ( row > 0 ) {
    result = node.writeSingleCoil(address, state);
    if ( ( address == 255 || address == 254 ) && state && result == node.ku8MBResponseTimedOut ){  
      result = 0;  //reset controller, timeout expected
    }
    #if DEBUG_ON>4
      debugMsg("Result: "+String(result));
    #endif
    return result; 
  } else {
    return -1;
  }
}

int MBus_get_int(int address, int &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_int: "+String(address));
  #endif
  uint8_t result = node.readHoldingRegisters(address, 1);
  if (result == node.ku8MBSuccess)  {
    uint16_t val_16; 
    val_16 = node.getResponseBuffer(0);
    value = reinterpret_cast<int16_t&>(val_16);             // change unsigned to signed
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

int MBus_get_uint16(int address, uint16_t &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_uint16: "+String(address));
  #endif
  uint8_t result = node.readHoldingRegisters(address, 1);
  if (result == node.ku8MBSuccess)  {
    value = node.getResponseBuffer(0) ;
  }
  #if DEBUG_ON>3
    debugMsg("Got "+String(value));
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

int MBus_get_n10(int address, float &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_n10: "+String(address));
  #endif
  uint16_t intval;
  int result = MBus_get_uint16(address, intval);
  if (result == node.ku8MBSuccess) {
    value = intval/10.;
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}
  
int MBus_get_uint32(int address, uint32_t &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_uint32: "+String(address));
  #endif
  int result = node.readHoldingRegisters(address, 2);
  if (result == node.ku8MBSuccess) {
    value = node.getResponseBuffer(0) << 16 ;         // HI first
    value += node.getResponseBuffer(1) ;
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

  
int MBus_get_uint32_rev(int address, uint32_t &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_uint32: "+String(address));
  #endif
  int result = node.readHoldingRegisters(address, 2);
  if (result == node.ku8MBSuccess) {
    value = node.getResponseBuffer(1) << 16 ;         // LO first
    value += node.getResponseBuffer(0) ;
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

int MBus_get_dn10(int address, float &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_dn10: "+String(address));
  #endif
  uint32_t intval;
  int result = MBus_get_uint32(address, intval);
  if (result == node.ku8MBSuccess) {
    value = intval/10.;
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}  

int MBus_get_float(int address, float &value) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_float: "+String(address));
  #endif
  int result = node.readHoldingRegisters(address, 1);
  if (result == node.ku8MBSuccess) {
    uint16_t data = node.getResponseBuffer(0);
    value = IEEEf16::f32(data);
  }
  #if DEBUG_ON>3
    debugMsg("Result: "+String(result));
  #endif
  return result;
}

int MBus_get_reg(int address, String &value) {         // given an address, looks up type and gets value. returns 0 on success
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_reg: "+String(address));
  #endif
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
      delay(i*50); // increasing delay between failed attempts 
    }  // try 3 times
    return result;
  } else {
    return -1;   
  } 
}

int mbGetFullReg(fullReg &myReg, int address) {  //given address, gets all we know about it
  if (noController) {
    myReg.row = 0; 
    myReg.var = "";
    myReg.desc = "";
    myReg.unit = noUnit;
    myReg.type = noType;
    myReg.value = "";
    return -1;
  }
  #if DEBUG_ON>3
    debugMsg("MBus_get_fullreg: "+String(address));
  #endif
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

int MBus_write_reg(int address, String valu) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_write_reg: "+String(address)+", "+String(valu));
  #endif
  int row;
  uint16_t foo_int;
  int foo_sint;
  float foo_fl;
  uint32_t foo_dint;
  uint16_t value1;
  uint16_t value2;
  int result = -1; // failure unless we say otherwise
    
  row = getMbRegIndex(address);
  #if DEBUG_ON>4
    debugMsg("type "+String(mbRegTypeName[mbRegType[row]]));
  #endif 
  switch (mbRegType[row]) {
    case f16:     value1 = IEEEf16::f16(valu.toFloat());
                  #if DEBUG_ON>4
                    foo_fl = IEEEf16::f32(value1);
                    debugMsg("writing 0x"+String(value1, HEX)+", "+String(foo_fl));
                  #endif
                  result = node.writeSingleRegister(address, value1);
                  break;
    case n10:     value1 = valu.toFloat()*10;
                  #if DEBUG_ON>4
                    debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/10");
                  #endif
                  result = node.writeSingleRegister(address, value1);
                  break;
    case dn10:    break;    // TODO ??
    case sigint:  ;
    case usigint: value1 = valu.toInt();
                  #if DEBUG_ON>4
                    debugMsg("Writing 0x"+String(value1, HEX)+", "+String(value1));
                  #endif
                  result = node.writeSingleRegister(address, value1);
                  break;
    case dint:    break;   // TODO ??
    case bitfield: break;
    case dbitfield: break;
    case bcd:     break;
// datatypes below are untested    
    case n673:  value1 = static_cast<int>(valu.toFloat() * 673); // SSDuo datatype, n/673 
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/673");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n1032: value1 = static_cast<int>(valu.toFloat() * 1032); // SSDuo datatype, n/1032
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/1032");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n1800: value1 = static_cast<int>(valu.toFloat() * 1800); // SSDuo datatype, n/1800
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/1800");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case r417:  break; // SSDuo datatype, % 0-417 ??? TODO:test result
    case n100:  value1 = static_cast<int>(valu.toFloat() * (32768/100)); // SS-MPPT datatype, n·100·2^-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/100)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n7916: value1 = static_cast<int>(valu.toFloat() * (32768/79.16)); // SS-MPPT datatype, n·79.16·2^-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/79.16)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n9895: value1 = static_cast<int>(valu.toFloat() * (65536/989.5)); // SS-MPPT datatype, n·989.5·2^-16
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(65536/989.5)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n96667: value1 = static_cast<int>(valu.toFloat() * (32768/96.667)); // SS-MPPT datatype, n·96.667^2-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/96.667)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n13915: value1 = static_cast<int>(valu.toFloat() * (32768/139.15)); // TS datatype, n·139.15·2^-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/139.15)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n66667: value1 = static_cast<int>(valu.toFloat() * (32768/66.667)); // TS datatype, n·66.667·2^-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/66.667)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n31667: value1 = static_cast<int>(valu.toFloat() * (32768/316.67)); // TS datatype, n·316.67·2^-15
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(32768/316.67)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n9616: value1 = static_cast<int>(valu.toFloat() * (65536/96.667)); // TS datatype, n·96.667·2-16
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(65536/96.667)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
    case n1008: value1 = static_cast<int>(valu.toFloat() * (256.0/100.0)); // SS-MPPT datatype, n*100*2^-8
                #if DEBUG_ON>4
                  debugMsg("writing 0x"+String(value1, HEX)+", "+String(value1)+"/(256.0/100.0)");
                #endif
                result = node.writeSingleRegister(address, value1);
                break;
      // TODO f32             
    default:           ;
  }
  #if DEBUG_ON>4
    debugMsg("Result: "+String(result));
  #endif
  return result; 
}

int MBus_get_reg_raw(int address, uint16_t &raw) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_reg_raw: "+String(address));
  #endif
  int result = node.readHoldingRegisters(address, 1);
  if (result == node.ku8MBSuccess) {
    raw = node.getResponseBuffer(0);
  }  
  return result;
}

int MBus_get_regs_raw(int address, uint16_t *rawarray, int count) {
  if (noController) return -1;
  #if DEBUG_ON>3
    debugMsg("MBus_get_regs_raw: "+String(count)+"x, "+String(address));
  #endif
  int result = 0;
  int passes = count / 32; // modbusmaster buffer is 64 bytes
  int leftover = count %32;
  #if DEBUG_ON>3
    debugMsg("RegsRaw, passes "+String(passes)+", left:"+String(leftover));
  #endif
  int i;
  for (i = 0; i < passes; i++) {
//    delay(5);
    result = node.readHoldingRegisters(address+(i*32), 32);
    for (int j = 0; j<32; j++) {
        rawarray[(i*32)+j] = node.getResponseBuffer(j);
        #if DEBUG_ON>3
          debugMsg("rawarray "+String((i*32)+j)+"="+String(node.getResponseBuffer(j),HEX));
        #endif
    }
  }
//  delay(5);
  result += node.readHoldingRegisters(address+i*32, leftover);
  for ( i=0; i<leftover; i++) {
    rawarray[passes*32+i] = node.getResponseBuffer(i);
  }
  return result;
}

int getLogItem(logItem &item, int idx) {   // idx starts at 0, returns 0 on success
  if (noController) return -1;
  uint16_t startReg = 32768 + (idx * 16);
  int result = MBus_get_uint32_rev(startReg, item.hourmeter);
  #if DEBUG_ON>3
    debugMsgContinue(F("getLogItem, startReg=0x"));
    debugMsg(String(startReg,HEX));
    debugMsgContinue(F("getLogItem, hourmeter="));
    debugMsg(String(item.hourmeter)); 
  #endif 
  delay(1);
  result += MBus_get_uint32_rev(startReg+2, item.alarm_daily);
  delay(1);
  result += MBus_get_uint32_rev(startReg+4, item.load_fault_daily);
  delay(1);
  result += MBus_get_uint32_rev(startReg+6, item.array_fault_daily);
  delay(1);
  result += MBus_get_uint16(startReg+8, item.Vb_min_daily);
  delay(1);
  result += MBus_get_uint16(startReg+9, item.Vb_max_daily);
  delay(1);
  result += MBus_get_uint16(startReg+10, item.Ahc_daily);
  delay(1);
  result += MBus_get_uint16(startReg+11, item.Ahl_daily);
  delay(1);
  result += MBus_get_uint16(startReg+12, item.Va_max_daily);
  delay(1);
  result += MBus_get_uint16(startReg+13, item.time_ab_daily);
  delay(1);
  result += MBus_get_uint16(startReg+14, item.time_eq_daily);
  delay(1);
  result += MBus_get_uint16(startReg+15, item.time_fl_daily);
  return result;
}

int getLog() {
  int result = 0;
  for (int i=0 ; i< 256 ; i++ ) {
    result += getLogItem(mbLog[i],i);
  }
  return result;
}

int readDeviceID(String &vendorName, String &productCode, String &majorMinorRevision) {
  unsigned char query[] = {(uint8_t)mbAddr, 0x2b, 0x0e, 0x01, 0x00, 0x00, 0x00};
  char vendor[32], product[32], revision[32], response[128];
  uint8_t count, id, id_idx, numObjs;
  uint16_t crc = 0xffff;
  for (int i=0 ; i < 5 ; i++) {
    crc = crc16_update(crc, query[i]);
  }
  query[5] = (uint8_t)(crc & 0x00FF);
  query[6] = (uint8_t)((crc & 0xFF00) >> 8);
  #if DEBUG_ON>3
    debugMsg("MbusCRC = " + String(crc,HEX));
  #endif  

  preTransmission();
  // flush receive buffer before transmitting request
  #ifdef ARDUINO_ARCH_ESP8266
    while (Serial.read() != -1);
    count = Serial.write(query, 7);
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    while (mbSerial.read() != -1);
    count = mbSerial.write(query, 7);
  #endif

  #if DEBUG_ON>4
    debugMsg("readDeviceID, wrote " + String(count));
  #endif

  //TODO is flush different? TX on 8266, RX on 32???
  #ifdef ARDUINO_ARCH_ESP8266
    Serial.flush();
  #endif
  #ifdef ARDUINO_ARCH_ESP32
    mbSerial.flush();
  #endif
  postTransmission();
  int startwait = 300; // up to 300 ms for start of a response.
  int charwait = 10;   // no more than 10 ms between chars, should come ~1 ms apart
  // loop until we run out of time or bytes, or an error occurs
  uint32_t doneTime = millis() + startwait; 
  bool done = false;
  count = 0;
  while ( !done ) {
    #ifdef ARDUINO_ARCH_ESP8266
      if (Serial.available()) {
        response[count++] = Serial.read();
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      if (mbSerial.available()) {
        response[count++] = mbSerial.read();
    #endif
    #if DEBUG_ON>4
      debugMsg("readDeviceID, response char=" + String(response[count-1],HEX));
    #endif
        doneTime = millis() + charwait; 
      }  //matches xx.available
    if (doneTime < millis()) { done = true; }  // no response or didn't get a char for a while
  } // while() - done getting response

  if (count == 0) {return -1;} else {count = 0;}
  id_idx = 8;
  numObjs = response[id_idx-1];
  vendorName = "";
  productCode = "";
  majorMinorRevision = "";
  for (int i = 0; i < numObjs ; i++) {
    id = response[id_idx]; // which object
    count = response[id_idx+1]; // length
    #if DEBUG_ON>3
      debugMsg("readDeviceID, id="+String(id));
    #endif
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
  
  #if DEBUG_ON>3
    debugMsgContinue(F("readDeviceID, vendorName="));
    debugMsg(vendorName);
    debugMsgContinue(F("readDeviceID, productCode="));
    debugMsg(productCode);
    debugMsgContinue(F("readDeviceID, majorMinorRevision="));
    debugMsg(majorMinorRevision);    
  #endif

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
  if (mbClient.available() > 0) {
    int i;
    for ( i = 0; i < 6; i++) {
      mbbuffer[i] = mbClient.read();
    }
    len = mbbuffer[4]*256 + mbbuffer[5];
    for ( i = 0 ; i < len ; i++ ) {
      mbbuffer[i+6] = mbClient.read();
    }
    func = mbbuffer[7];
    buffsize = 6 + len;
    #if DEBUG_ON>3
      debugMsgContinue(F("mbTCP received"));
      for (i=0;i<6+len;i++){
        debugMsgContinue("."+String(mbbuffer[i],HEX));
      }
      debugMsg("");
    #endif  
    
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
      // good response is to just return what we got...

    } else if (func == 6) { //write single register
      result = 0;
      refNum = mbbuffer[8]*256 + mbbuffer[9];   //register
      val    = mbbuffer[10]*256 + mbbuffer[11]; //value
      result = node.writeSingleRegister(refNum, val);
      // good response is to just return what we got...

    } else if (func == 43) { // Read device ID
      String vendorName, productCode, majorMinorRevision;
      vendorName.reserve(64);
      productCode.reserve(64);
      majorMinorRevision.reserve(64);
      result = readDeviceID(vendorName, productCode, majorMinorRevision);
      #if DEBUG_ON>4
        debugMsgContinue(F("MbusTCP, vendorName="));
        debugMsg(vendorName);
        debugMsgContinue(F("MbusTCP, productCode="));
        debugMsg(productCode);
        debugMsgContinue(F("MbusTCP, majorMinorRevision="));
        debugMsg(majorMinorRevision);
      #endif  

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
      #if DEBUG_ON>3
        debugMsgContinue(F("mbTCP sending"));
        for (i=0;i<buffsize;i++){
          debugMsgContinue("."+String(mbbuffer[i],HEX));
        }
        debugMsg("");
      #endif  
      mbClient.write((const uint8_t *)mbbuffer, buffsize);
    } else {
      ; // TODO send error response
    }
  }
}

String getModel() {
  String vendorName, productCode, majorMinorRevision, mod = "", result;
  vendorName.reserve(64);
  productCode.reserve(64);
  majorMinorRevision.reserve(64);
  result = readDeviceID(vendorName, productCode, majorMinorRevision);
  #if DEBUG_ON>4
    debugMsgContinue(F("getModel, readDeviceID returned:"));
    debugMsg(String(result));
  #endif
  
  /* Model names
   * PS-PWM*, PS-MPPT*, SSDuo (SunSaver Duo), SS-MPPT*, TS-45 or TS-60 (Tristar),
   * TS-MPPT-60-600V-48, TS-MPPT-30, TS-MPPT-45, TS-MPPT-60,
   */
  if (productCode.startsWith(F("PS-PWM"))) {            mod = "PS-PWM"; }
  if (productCode.startsWith(F("PS-MPPT"))) {           mod = "PS-MPPT"; }
  if (productCode.startsWith(F("SS-MPPT"))) {           mod = "SS-MPPT"; }
  if (productCode.startsWith(F("SSDuo"))) {             mod = "SSDuo"; }
  if (productCode.startsWith(F("TS-MPPT-60-600V"))) {
                                                        mod = "TS-600"; 
  } else if (productCode.startsWith(F("TS-MPPT")))  {
                                                        mod = "TS-MPPT";
  } else if (productCode.startsWith(F("TS-"))) {
                                                        mod = "TS"; 
  }
  #if DEBUG_ON>1
    debugMsgContinue(F("getModel received:"));
    debugMsg(productCode);
    debugMsgContinue(F("getModel selected:"));
    debugMsg(mod);
  #endif
  return mod;
}

void checkController() {
  String lastModel = model;
  model = getModel();
  if (model == "") {
    #if DEBUG_ON>0
      debugMsg(F("No controller found."));
    #endif  
    noController = true;
  } else {
    #if DEBUG_ON>0
      debugMsgContinue(F("Controller found:"));
      debugMsg(model);
    #endif  
    if (lastModel != model) {             // model changed
      noController = false;
      void getFile(String model); // fwd declaration
      getFile(model);
    }
  }
}

bool controllerNeedsReset () {
  bool reset = false;
  uint16_t raw=0;
  int reg=0;
  int i;
  reg = findAddrByVar(F("load_fault"));
  MBus_get_reg_raw(reg, raw);
  for (i=0 ; i<32 ; i++) {
    if ( mbLoadMsg[i].startsWith(F("EEPROM")) ) {
      if ( raw & (0x0001 << i) ) reset = true;
    }
  }
  
  reg = findAddrByVar(F("array_fault"));
  MBus_get_reg_raw(reg, raw);
  for (i=0 ; i<32 ; i++) {
    if ( mbLoadMsg[i].startsWith(F("EEPROM")) ) {
      if ( raw & (0x0001 << i) ) reset = true;
    }
  }
  return reset;
}
