/*
 * Configure and start the web server
 */

#ifdef PROGMEM_FILES
  #include "PROGMEM_FILES.h"
#endif

bool serveFile(String path, AsyncWebServerRequest *request) {  
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  dataType = getMIMEtype(path);
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
  
  server.on("/chart", HTTP_GET, [](AsyncWebServerRequest *request){
    chartPageHandler(request);
    //request->redirect("/Chart/Chart.html");
  });

  rrdHandler = server.on("/controller.rrd", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect(ctlRrdFileName.c_str());
  });

  drrdHandler = server.on("/controllerd.rrd", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect(ctlDRrdFileName.c_str());
  });

  server.on("/platform", HTTP_GET, [](AsyncWebServerRequest *request){
    return platformPageHandler(request);
  });
  
  server.on("/setTime", HTTP_GET, [](AsyncWebServerRequest *request){
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return setTimePageHandler(request);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return cmdPageHandler(request);
  });

  server.on("/rootcmd", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if ( !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return rootCmdPageHandler(request);
  });
  
  server.on("/setcharge", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return setChargePageHandler(request);
  });

  server.on("/setother", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    setOtherPageHandler(request);
  });

  server.on("/rest", [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
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
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return wlanPageHandler(request);
  });

  server.on("/security_config", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) ) {
//      return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return securityPageHandler(request);
  });

  server.on("/logging_config", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) ) {
//      return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return loggingPageHandler(request);
  });

  server.on("/utility", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
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
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return resetPageHandler(request);
  });

  server.on("/resetall", HTTP_GET, [](AsyncWebServerRequest *request) {
//    if(!request->authenticate(generateDigestHash(root_username.c_str(), root_password.c_str(), my_hostname.c_str()).c_str()) \
//      && !request->authenticate(generateDigestHash(web_username.c_str(), web_password.c_str(), my_hostname.c_str()).c_str()) ) {
//     return request->requestAuthentication(my_hostname.c_str(), true);
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return resetAllPageHandler(request);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {  // If the client requests any URI
    debugMsgln("File not found on flash, trying SD Card: " + request->url(),4);
    sdPageHandler(request->url(), request);
  });

  // cache for 12 hours. ).setCacheControl("max-age=43200")
  server.serveStatic("/js/javascriptrrd.wlibs.js", SD, "/js/javascriptrrd.wlibs.js").setCacheControl("max-age=43200");  
  server.serveStatic("/ctl/",             FILESYSTEM, "/ctl/").setCacheControl("max-age=43200");
  server.serveStatic("/img/",             FILESYSTEM, "/img/").setCacheControl("max-age=43200");
//  server.serveStatic("/ace.js",           FILESYSTEM, "/ace.js").setCacheControl("max-age=43200");
//  server.serveStatic("/jquery.min.js",    FILESYSTEM, "/jquery.min.js").setCacheControl("max-age=43200");
//  server.serveStatic("/mode-html.js",     FILESYSTEM, "/mode-html.js").setCacheControl("max-age=43200");
  server.serveStatic("/favicon.ico",      FILESYSTEM, "/favicon.ico").setCacheControl("max-age=43200");
  #ifndef PROGMEM_FILES
    server.serveStatic("/js/local.js",         FILESYSTEM, "/js/local.js").setCacheControl("max-age=43200");
    server.serveStatic("/local.css",        FILESYSTEM, "/local.css").setCacheControl("max-age=43200");
  #endif  
  server.serveStatic("/",                   FILESYSTEM, "/");   // everything else in flash

  server.on("/sd/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    debugMsgln("server.on(/sd/): " + request->url(),3);
    sdPageHandler(request->url(), request);
  });

  #ifdef PROGMEM_FILES
    //#define PREFER_SD  // define to check for files on SD first
    server.on("/js/OTA.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
      #ifdef PREFER_SD
      if (sd_card_available && loadFromSdCard(request->url(), request)) {   // look on SD Card first
        debugMsgln("server.on(/js/OTA.js from SD Card)",3);
      } else 
      #endif
      {  
        debugMsgln("server.on(/js/OTA.js from PROGMEM)",3);
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", OTA_js_gz, sizeof(OTA_js_gz));
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=43200");
        request->send(response);
      }
    });
    
    server.on("/js/local.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
      #ifdef PREFER_SD
      if (sd_card_available && loadFromSdCard(request->url(), request)) {   // look on SD Card first
        debugMsgln("server.on(/local.js from SD Card)",3);
      } else 
      #endif
      {
        debugMsgln("server.on(/local.js from PROGMEM)",3);
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", local_js, sizeof(local_js));
        response->addHeader("Cache-Control", "max-age=43200");
        request->send(response);
      }
    });

    server.on("/local.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
      #ifdef PREFER_SD
      if (sd_card_available && loadFromSdCard(request->url(), request)) {   // look on SD Card first
        debugMsgln("server.on(/local.css from SD Card)",3);
      } else 
      #endif
      {
        debugMsgln("server.on(/local.css from PROGMEM)",3);
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", local_css, sizeof(local_css));
        response->addHeader("Cache-Control", "max-age=43200");
        request->send(response);
      }
    });
    
    server.on("/js/draw.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
      #ifdef PREFER_SD
      if (sd_card_available && loadFromSdCard(request->url(), request)) {   // look on SD Card first
        debugMsgln("server.on(/draw.js from SD Card)",3);
      } else 
      #endif
      {
        debugMsgln("server.on(/draw.js from PROGMEM)",3);
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", draw_js, sizeof(draw_js));
        response->addHeader("Cache-Control", "max-age=43200");
        request->send(response);
      }
    });

  #endif

  asynchOTA.begin(&server, root_username.c_str(), root_password.c_str());
  asynchOTA.setID(my_hostname.c_str());

  server.on("/update", HTTP_GET, [&](AsyncWebServerRequest *request){
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return updatePageHandler(request);
  });

  debugMsgln("ESP32 server.ons done",2);

  server.begin();
  debugMsgln(F("HTTP server started"),1);

}
