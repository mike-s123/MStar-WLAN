/* This is mostly from:
  FSWebServer - Example WebServer with FS backend for esp8266/esp32
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + " KiB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + " MiB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GiB";
  }
}

bool exists(String path){
  bool yes = false;
  File file = FILESYSTEM.open(path, "r");
  if(!file.isDirectory()){
    yes = true;
  }
  file.close();
  return yes;
}

String getContentType(String filename); // fwd


#ifdef ARDUINO_ARCH_ESP8266
  bool handleFileRead(String path) {
    #if DEBUG_ON>2
      debugMsg("handleFileRead: " + path);
    #endif
    if (path.endsWith("/")) {
      path += "index.htm";
    }
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
      if (SPIFFS.exists(pathWithGz)) {
        path += ".gz";
      }
      File file = SPIFFS.open(path, "r");
      server.streamFile(file, contentType);
      file.close();
      return true;
    }
    return false;
  }
  
  void handleFileUpload() {
    if (server.uri() != "/edit") {
      return;
    }
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      String filename = upload.filename;
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      #if DEBUG_ON>2
        debugMsgContinue("handleFileUpload Name: "); debugMsg(filename);
      #endif
      fsUploadFile = SPIFFS.open(filename, "w");
      filename = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
      if (fsUploadFile) {
        fsUploadFile.write(upload.buf, upload.currentSize);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (fsUploadFile) {
        fsUploadFile.close();
      }
      #if DEBUG_ON>2
        debugMsgContinue("handleFileUpload Size: "); debugMsg(String(upload.totalSize));
      #endif
    }
  }
  
  void handleFileDelete() {
    if (server.args() == 0) {
      return server.send(500, "text/plain", "BAD ARGS");
    }
    String path = server.arg(0);
    #if DEBUG_ON>2
      debugMsg("handleFileDelete: " + path);
    #endif
    if (path == "/") {
      return server.send(500, "text/plain", "BAD PATH");
    }
    if (!SPIFFS.exists(path)) {
      return server.send(404, "text/plain", "FileNotFound");
    }
    SPIFFS.remove(path);
    server.send(200, "text/plain", "");
    path = String();
  }
  
  void handleFileCreate() {
    if (server.args() == 0) {
      return server.send(500, "text/plain", "BAD ARGS");
    }
    String path = server.arg(0);
    #if DEBUG_ON>2
      debugMsg("handleFileCreate: " + path);
    #endif
    if (path == "/") {
      return server.send(500, "text/plain", "BAD PATH");
    }
    if (SPIFFS.exists(path)) {
      return server.send(500, "text/plain", "FILE EXISTS");
    }
    File file = SPIFFS.open(path, "w");
    if (file) {
      file.close();
    } else {
      return server.send(500, "text/plain", "CREATE FAILED");
    }
    server.send(200, "text/plain", "");
    path = String();
  }
  
  void handleFileList() {
    if (!server.hasArg("dir")) {
      server.send(500, "text/plain", "BAD ARGS");
      return;
    }
  
    String path = server.arg("dir");
    #if DEBUG_ON>2
      debugMsg("handleFileList: " + path);
    #endif
    Dir dir = SPIFFS.openDir(path);
    path = String();
  
    String output = "[";
    while (dir.next()) {
      File entry = dir.openFile("r");
      if (output != "[") {
        output += ',';
      }
      bool isDir = false;
      output += "{\"type\":\"";
      output += (isDir) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(entry.name()).substring(1);
      output += "\"}";
      entry.close();
    }
  
    output += "]";
    server.send(200, "text/json", output);
  }
#endif // ARDUINO_ARCH_ESP8266


#ifdef ARDUINO_ARCH_ESP32

bool handleFileRead(String path) {
  #if DEBUG_ON>2
    debugMsg("handleFileRead: " + path);
  #endif
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    File file = FILESYSTEM.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    #if DEBUG_ON>2
      debugMsgContinue("handleFileUpload Name: "); debugMsg(filename);
    #endif
    fsUploadFile = FILESYSTEM.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    #if DEBUG_ON>2
      debugMsgContinue("handleFileUpload Size: "); debugMsg(String(upload.totalSize));
    #endif
  }
}

void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  #if DEBUG_ON>2
  debugMsg("handleFileDelete: " + path);
  #endif
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  FILESYSTEM.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  #if DEBUG_ON>2
    debugMsg("handleFileCreate: " + path);
  #endif
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = FILESYSTEM.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  #if DEBUG_ON>2
    debugMsg("handleFileList: " + path);
  #endif

  File root = FILESYSTEM.open(path);
  path = String();

  String output = "[";
  if(root.isDirectory()){
      File file = root.openNextFile();
      while(file){
          if (output != "[") {
            output += ',';
          }
          output += "{\"type\":\"";
          output += (file.isDirectory()) ? "dir" : "file";
          output += "\",\"name\":\"";
          output += String(file.name()).substring(1);
          output += "\"}";
          file = root.openNextFile();
      }
  }
  output += "]";
  server.send(200, "text/json", output);
}
#endif
