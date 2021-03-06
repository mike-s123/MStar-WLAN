
// functions to read the controller .csv file

void getRegFromFile(String model) {
  debugMsgln("Reading registers from /ctl/" + model + ".csv",3);
  File regFile = FILESYSTEM.open("/ctl/" + model + ".csv", "r");
  if (!regFile) {
    debugMsgln("register file not found!",1);
    return;
  } else {
    int row = 1;
    boolean eof = false;
    regFile.seek(0);
    while ( row <= REG_ROWS && !eof) {
      char received;
      String line;
      line = "";
      do {
        received = regFile.read();
        if ( received == '\r') {
          received = regFile.read();  // skip CR
        }
        if ( received == '\\' ) {
          eof = true;  // backslash ends file
          mbRegAddr[row] = -1;
        }
        else if ( received != '\n') {
          line += received;
        }
      } while ( received != '\n' && !eof );
      if ( !eof ) {
        if ( line.startsWith("r,",0) ) {      // only do register lines         
          debugMsgln("row=" + String(row),6);
          debugMsgln("line=" + line,6);
          int pos = 2;                                   // skip the "r,"
          int delim = 1;         
          for (int i = 1 ; i <= 6 ; i++) {
            delim = line.indexOf(',', pos);
            String val = line.substring(pos, delim);
            switch (i) {
              case 1: mbRegAddr[row] = val.toInt() ;
                      mbRegMax = mbRegAddr[row]>mbRegMax?mbRegAddr[row]:mbRegMax;
                break;
              case 2: mbRegVar[row] = val;
                break;
              case 3: mbRegDesc[row] = val;
                break;
              case 4: if (val == "A") {
                  mbRegUnit[row] = Amp;
                } else if (val == "V") {
                  mbRegUnit[row] = Volt;
                } else if (val == "Ah") {
                  mbRegUnit[row] = Ah;
                } else if (val == "kWh") {
                  mbRegUnit[row] = kWh;
                } else if (val == "W") {
                  mbRegUnit[row] = Watt;
                } else if (val == "C") {
                  mbRegUnit[row] = Celsius;
                } else if (val == "secs") {
                  mbRegUnit[row] = seconds;
                } else if (val == "hours") {
                  mbRegUnit[row] = hours;
                } else if (val == "days") {
                  mbRegUnit[row] = days;
                } else if (val == "%") {
                  mbRegUnit[row] = percent;
                } else {
                  mbRegUnit[row] = noUnit;
                }
                break;
              case 5: if (val == "f16") {
                  mbRegType[row] = f16;
                } else if (val == "n10") {
                  mbRegType[row] = n10;          // 1 word, n*0.1
                } else if (val == "dn10") {
                  mbRegType[row] = dn10;         // 2 words, n*0.1
                } else if (val == "uint") {
                  mbRegType[row] = usigint;
                } else if (val == "int") {
                  mbRegType[row] = sigint;
                } else if (val == "dint") {    // 2 words, unsigned int
                  mbRegType[row] = dint;
                } else if (val == "bitf") {    // 1 word bitfield
                  mbRegType[row] = bitfield;
                } else if (val == "dbitf") {   // 2 word bitfield
                  mbRegType[row] = dbitfield;
                } else if (val == "bcd") {    // binary coded decimal
                  mbRegType[row] = bcd;
                } else if (val == "n673") {    // 
                  mbRegType[row] = n673;
                } else if (val == "n1032") {    // 
                  mbRegType[row] = n1032;
                } else if (val == "n1800") {    // 
                  mbRegType[row] = n1800;
                } else if (val == "r417") {    // 
                  mbRegType[row] = r417;
                } else if (val == "n100") {    // 
                  mbRegType[row] = n100;
                } else if (val == "n79_16") {    // 
                  mbRegType[row] = n7916;
                } else if (val == "n989_5") {    // 
                  mbRegType[row] = n9895;
                } else if (val == "n96_667") {    // 
                  mbRegType[row] = n96667;
                } else if (val == "n139_15") {    // 
                  mbRegType[row] = n13915;
                } else if (val == "n66_667") {    // 
                  mbRegType[row] = n66667;
                } else if (val == "n316_67") {    // 
                  mbRegType[row] = n31667;
                } else if (val == "n96_16") {    // n·96.667·2^-16 note -16 scaling, negative sign is assumed
                  mbRegType[row] = n9616;
                } else if (val == "n100_8") {
                  mbRegType[row] = n1008;
                } else if (val == "f32") {
                  mbRegType[row] = f32;
                } else {
                  mbRegType[row] = noType;
                }
                break;
            case 6: mbRegRW[row] = (val=="RW") ? true : false;
                break;
            }
            pos = delim + 1;
          } // process line
          line = "";                              // clear and go get more
          row++;
        } // line not comment
      }  // not eof
    } // not eof, not too many rows
  }
  regFile.close();
}

void getCoilFromFile(String model) {
  debugMsgln("Reading coils from /ctl/" + model + ".csv",3);
  File coilFile = FILESYSTEM.open("/ctl/" + model + ".csv", "r");
  if (!coilFile) {
    debugMsgln(F("Coil file not found!"),1);
    return;
  } else {
    int row = 1;
    boolean eof = false;
    coilFile.seek(0);
    while ( row <= COIL_ROWS && !eof) {
      char received;
      String line;
      line = "";
      do {
        received = coilFile.read();
        if ( received == '\r') {
          received = coilFile.read();  // skip CR
        }
        if ( received == '\\' ) {
          eof = true;  // backslash ends file
          mbCoilAddr[row] = -1;
        }
        else if ( received != '\n') {
          line += received;
        }
      } while ( received != '\n' && !eof );
      if ( !eof ) {
        if ( line.startsWith("c,",0)   ) {      // only coil lines         
          debugMsgln("line=" + line,6);
          int pos = 2;                                   // skip the "c," 
          int delim = 1;
          for (int i = 1 ; i <= 3 ; i++) {
            delim = line.indexOf(',', pos);
            String val = line.substring(pos, delim);
            switch (i) {
              case 1: mbCoilAddr[row] = val.toInt() ;
                      mbCoilMax = mbCoilAddr[row]>mbCoilMax?mbCoilAddr[row]:mbCoilMax;
                break;
              case 2: mbCoilVar[row] = val;
                break;
              case 3: mbCoilDesc[row] = val;
                break;
            }
            pos = delim + 1;
          } // process line
          line = "";                              // clear and go get more
          row++;
        } // skip comment
      } // not eof
    } // not eof, not too many rows
  }
  coilFile.close();
}

void getAlarmFromFile(String model) {
  debugMsgln("Reading alarms from /ctl/" + model + ".csv",3);
  File alarmFile = FILESYSTEM.open("/ctl/" + model + ".csv", "r");
  if (!alarmFile) {
    debugMsgln(F("Alarm file not found!"),1);
    return;
  } else {
    int row = 0;
    boolean eof = false;
    alarmFile.seek(0);
    for ( row = 0 ; row <32 ; row++ ) { mbAlarmMsg[row]= F("n/a"); }  // set defaults  
    row = 0;  
    while ( row < 32 && !eof) {
      char received;
      String line;
      line = "";

      do {
        received = alarmFile.read();
        if ( received == '\r') {
          received = alarmFile.read();  // skip CR
        }
        if ( received == '\\' ) {
          eof = true;  // backslash ends file
        }
        else if ( received != '\n') {
          line += received;
        }
      } while ( received != '\n' && !eof );

      if ( !eof ) {
        if ( line.startsWith("m,",0)  ) {      // alarm lines start with "m,"        
          debugMsgln("alarm line=" + line,6);
          int pos = 2;                                   // process line
          int delim = 1;
          for (int i = 1 ; i <= 2 ; i++) {
            delim = line.indexOf(',', pos);
            String val = line.substring(pos, delim);
            int myBit;
            switch (i) {
              case 1: myBit = val.toInt();
                break;
              case 2: mbAlarmMsg[myBit] = val;
                      debugMsgln("set mbAlarmMsg-" + String(myBit) + ": " + mbAlarmMsg[myBit],6);
                break;
            }
            pos = delim + 1;
          } // process line
          line = "";                              // clear and go get more
          row++;
        } // skip comment
      } // not eof
    } // not eof, not too many rows
  }
  alarmFile.close();
}

void getArrayFromFile(String model) {
  debugMsgln("Reading array faults from /ctl/" + model + ".csv",3);
  File arrayFile = FILESYSTEM.open("/ctl/" + model + ".csv", "r");
  if (!arrayFile) {
    debugMsgln(F("Array file not found!"),1);
    return;
  } else {
    int row = 0;
    boolean eof = false;
    arrayFile.seek(0);
    for ( row = 0 ; row <32 ; row++ ) { mbArrayMsg[row]= F("n/a"); }  // set defaults  
    row = 0;  
    while ( row < 32 && !eof) {
      char received;
      String line;
      line = "";

      do {
        received = arrayFile.read();
        if ( received == '\r') {
          received = arrayFile.read();  // skip CR
        }
        if ( received == '\\' ) {
          eof = true;  // backslash ends file
        }
        else if ( received != '\n') {
          line += received;
        }
      } while ( received != '\n' && !eof );

      if ( !eof ) {
        if ( line.startsWith("y,",0)  ) {      // array lines begin with "y,"         
          debugMsgln("array line=" + line,6);
          int pos = 2;                                   // process line
          int delim = 1;
          for (int i = 1 ; i <= 2 ; i++) {
            delim = line.indexOf(',', pos);
            String val = line.substring(pos, delim);
            int myBit;
            switch (i) {
              case 1: myBit = val.toInt();
                break;
              case 2: mbArrayMsg[myBit] = val;
                      debugMsgln("set mbArrayMsg-" + String(myBit) + ": " + mbArrayMsg[myBit],6);
                break;
            }
            pos = delim + 1;
          } // process line
          line = "";                              // clear and go get more
          row++;
        } // skip comment
      } // not eof
    } // not eof, not too many rows
  }
  arrayFile.close();
}

void getLoadFromFile(String model) {
  debugMsgln("Reading load faults from /ctl/" + model + ".csv",3);
  File loadFile = FILESYSTEM.open("/ctl/" + model + ".csv", "r");
  if (!loadFile) {
    debugMsgln(F("load file not found!"),1);
    return;
  } else {
    int row = 0;
    boolean eof = false;
    loadFile.seek(0);
    for ( row = 0 ; row <32 ; row++ ) { mbLoadMsg[row] = F("n/a"); }  // set defaults  
    row = 0;  
    while ( row < 32 && !eof) {
      char received;
      String line;
      line = "";

      do {
        received = loadFile.read();
        if ( received == '\r') {
          received = loadFile.read();  // skip CR
        }
        if ( received == '\\' ) {
          eof = true;  // backslash ends file
        }
        else if ( received != '\n') {
          line += received;
        }
      } while ( received != '\n' && !eof );

      if ( !eof ) {
        if ( line.startsWith("l,",0)  ) {      // load lines start with "l,"         
          debugMsgln("load line=" + line,6);
          int pos = 2;                                   // process line
          int delim = 1;
          for (int i = 1 ; i <= 2 ; i++) {
            delim = line.indexOf(',', pos);
            String val = line.substring(pos, delim);
            int myBit;
            switch (i) {
              case 1: myBit = val.toInt();
                break;
              case 2: mbLoadMsg[myBit] = val;
                      debugMsgln("set mbLoadMsg-" + String(myBit) + ": " + mbLoadMsg[myBit],6);
                break;
            }
            pos = delim + 1;
          } // process line
          line = "";                              // clear and go get more
          row++;
        } // skip comment
      } // not eof
    } // not eof, not too many rows
  }
  loadFile.close();
}

void getFile(String model) {
  getRegFromFile (model);
  getCoilFromFile(model);
  getLoadFromFile(model);
  getArrayFromFile(model);
  getAlarmFromFile(model);
}
