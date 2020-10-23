/*
 * 
 * Configure and start the web server
 * 
 */

// OTA update functions, from https://github.com/lbernstone/asyncUpdate
void handleUpdate(AsyncWebServerRequest *request) {
  debugMsgln(F("Entering /update page."),2);
  checkController();

  String response_message;
  response_message.reserve(1000);
  response_message = getHTMLHead();
  response_message += getNavBar();
  response_message += "<script> \
    function notify_update() {document.getElementById(\"update\").innerHTML = \"<h2>Updating...</h2>\"\; } \
    </script>";
  response_message += "Firmware = *.esp32.bin<br>SPIFFS = *.spiffs.bin<br> \
  <form method='POST' action='/doUpdate' enctype='multipart/form-data' target='_self' onsubmit='notify_update()'> \
  <input type='file' name='update'><br> \
  <input type='submit' value='Do update'></form> \
  <div id=\"update\"></div> \
  ";
  response_message += getHTMLFoot();
  request->send(200, "text/html", response_message);
};

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    size_t content_len;  
    debugMsgln(F("OTA Updating"),1);
    logFile.flush();
    content_len = request->contentLength();
    // if filename includes spiffs, update the fs partition
    int cmd = (filename.indexOf(F(".spiffs.bin")) > -1 ) ? U_SPIFFS : U_FLASH;
    if (cmd == U_FLASH && !(filename.indexOf(F("esp32.bin")) > -1) ) {
      debugMsgln("OTA bad filename",1);
      return; // wrong image for ESP32
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(DEBUG_ESP_PORT);
      debugMsgln("OTA .begin error",1);
    }
  }
  if (Update.write(data, len) != len) {
    Update.printError(DEBUG_ESP_PORT);
    debugMsgln("OTA .write len error",1);
  }
  if (final) {    
    if (!Update.end(true)){
      Update.printError(DEBUG_ESP_PORT);
      debugMsgln("OTA .end error",1);
    } else {
      String response_message;
      response_message = getHTMLHead();
      response_message += "<h2>Please wait while the device reboots</h2> \
      <meta http-equiv=\"refresh\" content=\"30;url=/\" />";
      response_message += getHTMLFoot();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", response_message);
      response->addHeader("Refresh", "30");  
      response->addHeader("Location", "/");
      request->send(response);    
      debugMsgln("Update complete, rebooting",1);
      if (logFile) logFile.flush();
      delay(100);
      ESP.restart();
    }
  }
}

bool serveFile(String path, AsyncWebServerRequest *request) {  
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm") || path.endsWith(".html")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }
  File dataFile = FILESYSTEM.open(path, "r");
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = FILESYSTEM.open(path, "r");
  }
  if (!dataFile) return false;
  dataFile.close();
  if (request->hasArg("download")) {
    dataType = "application/octet-stream";
  }
  request->send(FILESYSTEM, path, dataType);
  return true;
}

void startWeb() { 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    statusPageHandler(request);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    statusPageHandler(request);
  });
  
  server.on("/platform", HTTP_GET, [](AsyncWebServerRequest *request){
    return platformPageHandler(request);
  });
  
  server.on("/setTime", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return setTimePageHandler(request);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return cmdPageHandler(request);
  });

  server.on("/setcharge", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return setChargePageHandler(request);
  });

  server.on("/setother", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    setOtherPageHandler(request);
  });

  server.on("/rest", [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    restPageHandler(request);
  });

  server.on("/allregs", HTTP_GET, [](AsyncWebServerRequest *request){
    allregsPageHandler(request);
  });
  
  server.on("/allcoils", HTTP_GET, [](AsyncWebServerRequest *request){
    allcoilsPageHandler(request);
  });

  server.on("/wlan_config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return wlanPageHandler(request);
  });

  server.on("/security_config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return securityPageHandler(request);
  });

  server.on("/utility", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return utilityPageHandler(request);
  });

  server.on("/getfile", HTTP_GET, [](AsyncWebServerRequest *request){
    getfilePageHandler(request);
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return resetPageHandler(request);
  });

  server.on("/resetall", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
     && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return resetAllPageHandler(request);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {  // If the client requests any URI
    debugMsgln("server.onNotFound:" + request->url(),3);
    #ifdef ARDUINO_ARCH_ESP32
      if (request->url().startsWith("/sd/")) {              // if on sd card
        debugMsgln("server.on(/sd/):" + request->url(),3);
        sdPageHandler(request->url(), request);
        return;
      }
    #endif
    if (!serveFile(request->url(),request))                  // send it if it exists
      request->send(404, F("text/plain"), F("404: Not Found")); // otherwise, respond with a 404 (Not Found) error
  });

  // cache for 12 hours.
  server.serveStatic("/ctl/",       FILESYSTEM, "/ctl/",        "max-age=43200");
  server.serveStatic("/img/",       FILESYSTEM, "/img/",        "max-age=43200");
  server.serveStatic("/local.js",             FILESYSTEM, "/local.js",              "max-age=43200");
  server.serveStatic("/local.css",            FILESYSTEM, "/local.css",             "max-age=43200");
//  server.serveStatic("/ace.js",               FILESYSTEM, "/ace.js",                "max-age=43200");
//  server.serveStatic("/jquery.min.js",        FILESYSTEM, "/jquery.min.js",         "max-age=43200");
//  server.serveStatic("/mode-html.js",         FILESYSTEM, "/mode-html.js",          "max-age=43200");
  server.serveStatic("/favicon.ico",          FILESYSTEM, "/favicon.ico",           "max-age=43200");

  server.serveStatic("/",FILESYSTEM,"/"); // everything else in flash

  #ifdef ARDUINO_ARCH_ESP32
    debugMsgln("ESP32 server.ons",1);

    server.on("/sd/", HTTP_GET, [] (AsyncWebServerRequest *request) {
      debugMsgln("server.on(/sd/):" + request->url(),3);
      sdPageHandler(request->url(), request);
    });

#endif // ARDUINO_ARCH_ESP32

  server.on(update_path, HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    handleUpdate(request);}
  );
  
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    handleDoUpdate(request, filename, index, data, len, final);}
  );
  
//  Update.onProgress(printProgress);

  server.begin();
  debugMsgln(F("HTTP server started"),1);

}
