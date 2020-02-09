/*
 * 
 * Configure and start the web server
 * 
 */

void startWeb() { 
  server.on("/",               statusPageHandler);
  server.on("/status",      statusPageHandler);
  
  server.on("/platform", platformPageHandler);

  server.on("/setTime", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setTimePageHandler();
  });

  server.on("/cmd", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return cmdPageHandler();
  });

  server.on("/setcharge", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setChargePageHandler();
  });

  server.on("/setother", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return setOtherPageHandler();
  });

  server.on("/rest", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return restPageHandler();
  });

  server.on("/allregs",      allregsPageHandler);
  server.on("/allcoils",     allcoilsPageHandler);

  server.on("/wlan_config", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return wlanPageHandler();
  });
  server.on("/utility", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return utilityPageHandler();
  });

  server.on("/getfile",   getfilePageHandler);

  server.on("/reset", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return resetPageHandler();
  });

  server.on("/resetall", []() {
    if (!server.authenticate(web_username, web_password)) {
      return server.requestAuthentication();
    }
    return resetAllPageHandler();
  });

  server.on("/list", HTTP_GET, handleFileList);
  //load editor

  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  server.onNotFound([]() {                              // If the client requests any URI
    debugMsgln("server.onNotFound:" + server.uri(),3);
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, F("text/plain"), F("404: Not Found")); // otherwise, respond with a 404 (Not Found) error
  });
  
  // static for 12 hours.
  server.serveStatic("/ctl/PS-PWM.png",       FILESYSTEM, "/ctl/PS-PWM.png",        "max-age=43200");
  server.serveStatic("/ctl/PS-MPPT.png",      FILESYSTEM, "/ctl/PS-MPPT.png",       "max-age=43200"); 
  server.serveStatic("/ctl/SSDuo.png",        FILESYSTEM, "/ctl/SSDuo.png",         "max-age=43200"); 
  server.serveStatic("/ctl/SS-MPPT.png",      FILESYSTEM, "/ctl/SS-MPPT.png",       "max-age=43200"); 
  server.serveStatic("/ctl/TS.png",           FILESYSTEM, "/ctl/TS.png",            "max-age=43200"); 
  server.serveStatic("/ctl/TS-MPPT.png",      FILESYSTEM, "/ctl/TS-MPPT.png",       "max-age=43200"); 
  server.serveStatic("/ctl/TS-600.png",       FILESYSTEM, "/ctl/TS-600.png",        "max-age=43200"); 
  server.serveStatic("/ctl/Nocontroller.png", FILESYSTEM, "/ctl/Nocontroller.png",  "max-age=43200"); 
  #ifndef DEBUG_JS
    server.serveStatic("/local.js",             FILESYSTEM, "/local.js",              "max-age=43200");
  #endif 
  #ifndef DEBUG_CSS
    server.serveStatic("/local.css",            FILESYSTEM, "/local.css",             "max-age=43200");
  #endif
  server.serveStatic("/img/charging.png",     FILESYSTEM, "/img/charging.png",      "max-age=43200");
  server.serveStatic("/img/otherset.png",     FILESYSTEM, "/img/otherset.png",      "max-age=43200");
  server.serveStatic("/img/wrover.png",       FILESYSTEM, "/img/wrover.png",        "max-age=43200");
  server.serveStatic("/img/wemos.png",        FILESYSTEM, "/img/wemos.png",         "max-age=43200");
  server.serveStatic("/img/utility.png",      FILESYSTEM, "/img/utility.png",       "max-age=43200");
  server.serveStatic("/img/setTime.png",      FILESYSTEM, "/img/setTime.png",       "max-age=43200");
  server.serveStatic("/ace.js",               FILESYSTEM, "/ace.js",                "max-age=43200");
  server.serveStatic("/jquery.min.js",        FILESYSTEM, "/jquery.min.js",         "max-age=43200");
  server.serveStatic("/mode-html.js",         FILESYSTEM, "/mode-html.js",          "max-age=43200");
  server.serveStatic("/favicon.ico",          FILESYSTEM, "/favicon.ico",           "max-age=43200");


  #ifdef ARDUINO_ARCH_ESP32
    debugMsgln("ESP32 server.ons",1);
    #include <uri/UriBraces.h>
    server.on(UriBraces("/sd/{}"), []() {
      sdPageHandler(server.pathArg(0));
    });

  /*handling uploading firmware file */
    server.on(update_path, HTTP_GET, []() {
    if (!server.authenticate(update_username, update_password)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

#endif // ARDUINO_ARCH_ESP32
  
  server.begin();
  debugMsgln(F("HTTP server started"),1);

}
