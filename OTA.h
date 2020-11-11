/*
 
  OTA.h is based on  https://github.com/ayushsharma82/AsyncElegantOTA (v 2.2.5), MIT license
  portions are also used in the OTA.js file in the data directory

MIT License

Copyright (c) 2019 Ayush Sharma

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#ifndef AsyncMyOTA_h
#define AsyncMyOTA_h

#include "Arduino.h"
#include "stdlib_noniso.h"

#include "WiFi.h"
#include "AsyncTCP.h"
#include "Update.h"
//#include "esp_int_wdt.h"
//#include "esp_task_wdt.h"

//#include "Hash.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"

//#include "elegantWebpage.h"


class AsyncMyOtaClass{

    public:

        void setID(const char* id){
            _id = id;
        }

        void begin(AsyncWebServer *server, const char* username = "", const char* password = ""){
            _server = server;

            if(strlen(username) > 0){
                _authRequired = true;
                _username = username;
                _password = password;
            }else{
                _authRequired = false;
                _username = "";
                _password = "";
            }

            _server->on("/update/identity", HTTP_GET, [&](AsyncWebServerRequest *request){
                if(_authRequired){
                    if(!request->authenticate(_username.c_str(), _password.c_str())){
                        return request->requestAuthentication();
                    }
                }
                request->send(200, "application/json", "{\"id\": \""+_id+"\", \"hardware\": \"ESP32\"}");
            });
/*
            _server->on("/update", HTTP_GET, [&](AsyncWebServerRequest *request){
                if(_authRequired){
                    if(!request->authenticate(_username.c_str(), _password.c_str())){
                        return request->requestAuthentication();
                    }
                }
                AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ELEGANT_HTML, ELEGANT_HTML_SIZE);
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
            });
*/
            _server->on("/update", HTTP_POST, [&](AsyncWebServerRequest *request) {
                if(_authRequired){
                    if(!request->authenticate(_username.c_str(), _password.c_str())){
                        return request->requestAuthentication();
                    }
                }
                // the request handler is triggered after the upload has finished... 
                // create the response, add header, and send response
                AsyncWebServerResponse *response = request->beginResponse((Update.hasError())?500:200, "text/plain", (Update.hasError())?"FAIL":"OK");
                response->addHeader("Connection", "close");
                response->addHeader("Access-Control-Allow-Origin", "*");
                request->send(response);
                restartRequired = true;
            }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                //Upload handler chunks in data
                if(_authRequired){
                    if(!request->authenticate(_username.c_str(), _password.c_str())){
                        return request->requestAuthentication();
                    }
                }

                if (!index) {
                    if(!request->hasParam("MD5", true)) {
                        return request->send(400, "text/plain", "MD5 parameter missing");
                    }

                    if(!Update.setMD5(request->getParam("MD5", true)->value().c_str())) {
                        return request->send(400, "text/plain", "MD5 parameter invalid");
                    }
                    
/*                    debugMsgln("OTA filename:"+filename,1);

                    int cmd = (filename.indexOf(F(".spiffs.bin")) > -1 ) ? U_SPIFFS : U_FLASH;
                    if (cmd == U_FLASH && !(filename.indexOf(F("esp32.bin")) > -1) ) {
                      debugMsgln(F("OTA bad filename"),1);
                      return; // wrong image for ESP32
                    }
*/
                    int cmd = (filename == "filesystem") ? U_SPIFFS : U_FLASH;
                    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) { // Start with max available size
                      Update.printError(Serial);
                      return request->send(400, "text/plain", "OTA could not begin");
                    }

                }

                // Write chunked data to the free sketch space
                if(len){
                    if (Update.write(data, len) != len) {
                        return request->send(400, "text/plain", "OTA could not begin");
                    }
                }
                    
                if (final) { // if the final flag is set then this is the last frame of data
                    if (!Update.end(true)) { //true to set the size to the current progress
                        Update.printError(Serial);
                        return request->send(400, "text/plain", "Could not end OTA");
                    }
                }else{
                    return;
                }
            });
        }

        void loop(){
            if(restartRequired){
                yield();
                delay(1000);
                yield();
                ESP.restart();
            }
        }

    private:
        AsyncWebServer *_server;

        String getID(){
            String id = "";
            id = String((uint32_t)ESP.getEfuseMac(), HEX);
            id.toUpperCase();
            return id;
        }

        String _id = getID();
        String _username = "";
        String _password = "";
        bool _authRequired = false;
        bool restartRequired = false;

};

AsyncMyOtaClass AsyncMyOTA;
#endif
