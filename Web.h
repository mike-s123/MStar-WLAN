/*
 * 
 * Configure and start the web server
 * 
 */

bool serveFile(String path, AsyncWebServerRequest *request) {  
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
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

#ifdef PROGMEM_FILES
  #include "PROGMEM_FILES.h"
#endif

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
    debugMsgln("server.onNotFound:" + request->url(),3);
    debugMsgln("File not found on flash, trying SD Card:" + request->url(),3);
    sdPageHandler(request->url(), request);
  });

  // cache for 12 hours.
  server.serveStatic("/ctl/",       FILESYSTEM, "/ctl/",        "max-age=43200");
  server.serveStatic("/img/",       FILESYSTEM, "/img/",        "max-age=43200");
//  server.serveStatic("/ace.js",               FILESYSTEM, "/ace.js",                "max-age=43200");
//  server.serveStatic("/jquery.min.js",        FILESYSTEM, "/jquery.min.js",         "max-age=43200");
//  server.serveStatic("/mode-html.js",         FILESYSTEM, "/mode-html.js",          "max-age=43200");
  server.serveStatic("/favicon.ico",          FILESYSTEM, "/favicon.ico",           "max-age=43200");
  #ifndef PROGMEM_FILES
    server.serveStatic("/local.js",             FILESYSTEM, "/local.js",              "max-age=43200");
    server.serveStatic("/local.css",            FILESYSTEM, "/local.css",             "max-age=43200");
  #endif  
  server.serveStatic("/",FILESYSTEM,"/"); // everything else in flash

  debugMsgln("ESP32 server.ons",1);

  server.on("/sd/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    debugMsgln("server.on(/sd/):" + request->url(),3);
    sdPageHandler(request->url(), request);
  });

  #ifdef PROGMEM_FILES
    server.on("/OTA.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
      debugMsgln("server.on(/OTA.js from PROGMEM)",3);
      AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", OTA_js_gz, sizeof(OTA_js_gz));
      response->addHeader("Content-Encoding", "gzip");
      response->addHeader("Cache-Control", "max-age=43200");
      request->send(response);
    });
    server.on("/local.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
      debugMsgln("server.on(/local.js from PROGMEM)",3);
      AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", local_js, sizeof(local_js));
      response->addHeader("Cache-Control", "max-age=43200");
      request->send(response);
    });

    server.on("/local.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
      debugMsgln("server.on(/local.css from PROGMEM)",3);
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", local_css, sizeof(local_css));
      response->addHeader("Cache-Control", "max-age=43200");
      request->send(response);
    });
  #endif

  AsyncMyOTA.begin(&server, root_username.c_str(), root_password.c_str());    // Start
  AsyncMyOTA.setID(my_hostname.c_str());

  server.on("/update", HTTP_GET, [&](AsyncWebServerRequest *request){
    if (!request->authenticate(web_username.c_str(), web_password.c_str()) \
       && !request->authenticate(root_username.c_str(), root_password.c_str())) {
      return request->requestAuthentication();
    }
    return updatePageHandler(request);
  });

  server.begin();
  debugMsgln(F("HTTP server started"),1);

}
