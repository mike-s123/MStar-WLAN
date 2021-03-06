//-------------------------------------------------------------------
//------------------------- HTML support   --------------------------
//-------------------------------------------------------------------

/*
 * Much of this HTML stuff, along with some of the included "local.css" file, is by Ian Sparkes,
 * https://bitbucket.org/isparkes/nixiefirmwarev2/src
 * 
 * License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode
 */
String getLocalJs() {
  String js = F("<script src=\"/js/local.js\"></script>");
  return js;
}

String wrapScript(String script) {        // in: script out: script wrapped in tags
  String wrapped = F("<script>");
  wrapped += script;
  wrapped += F("</script>");
  return wrapped;
}

String getHTMLHead(int refresh=0) {
  String header = F("<!DOCTYPE html><html lang=\"en\"><head>");
  header += F("<link href=\"/local.css\" rel=\"stylesheet\">");
  header += getLocalJs();
  if (refresh) {
    header += "<meta http-equiv=\"refresh\" content=\"" + String(refresh) + "\">";
  }
  header += F("<title>");
  header += my_hostname;
  header += F("</title></head>");
  header += F("<body>");
  return header;
}

/*
String getAjaxHead() {                                      // like getHTMLHead, except includes jquery
  String header = F("<!DOCTYPE html><html><head>");
  header += F("<script src=\"/jquery.min.js\"></script>");
  header += getLocalJs();
  header += F("<link href=\"/local.css\" rel=\"stylesheet\">");
  header += F("<title>Modbus interface</title></head>");
  header += F("<body>");
  return header;
}
*/

String getNavBar() {
  
  String ctl;
    if (noController) {
    ctl = F("No Controller");
  } else {
    ctl = fullModel + " (" + ctlSerialNum + ")";
  }
  String navbar = F("<nav class=\"navbar navbar-inverse navbar-fixed-top\">");
  navbar += F("<div class=\"container-fluid\"><div class=\"navbar-header\">");
  navbar += F("<a class=\"navbar-brand\" href=\"/\">");
  navbar += my_hostname;
  navbar += F("</a></div>");
  navbar += F("<div class=\"navbar-brand\">");
  navbar += ctl;
  navbar += F("</div>");
  navbar += F("<div id=\"navbar\" class=\"navbar-collapse collapse\"><ul class=\"nav navbar-nav navbar-right\">");
  navbar += F("<li><a href=\"/status\">Status</a></li>");
  if (sd_card_available) navbar += F("<li><a href=\"/chart\">Chart</a></li>");  // chart only available with SD card
  navbar += F("<li><a href=\"/setcharge\">Charge Settings</a></li>");
  navbar += F("<li><a href=\"/setother\">Other Settings</a></li>");
  navbar += F("<li><a href=\"/platform\">Platform</a></li>");
  navbar += F("<li><a href=\"/utility\">Utility</a></li>");
  navbar += F("</ul></div></div></nav><br>");
  navbar += promptReset();
  return navbar;
}

String getFormHead(String formTitle) {
/**
   Get the header for an input form
*/
  String formHead = F("<div class=\"container\" role=\"main\"><h3 class=\"sub-header\">");
  formHead += formTitle;
  formHead += F("</h3><form class=\"form-horizontal\">");

  return formHead;
}

String getFormFoot() {
/**
   Get the footer for an input form
*/
  return F("</form></div>");
}

void handleNotFound(AsyncWebServerRequest *request) {
/* Called if requested page is not found */  
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += request->url();
  message += F("\nMethod: ");
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += request->args();
  message += "\n";

  for (uint8_t i = 0; i < request->args(); i++)
  {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }

  request->send(404, F("text/plain"), message);
}

String getTableHead2Col(String tableHeader, String col1Header, String col2Header) {
/*
   Get the header for a 2 column table
*/
  String tableHead = F("<div class=\"container\" role=\"main\"><h3 class=\"sub-header\">");
  tableHead += tableHeader;
  tableHead += F("</h3><div class=\"table-responsive\"><table class=\"table table-striped\"><thead><tr><th>");
  tableHead += col1Header;
  tableHead += F("</th><th>");
  tableHead += col2Header;
  tableHead += F("</th></tr></thead><tbody>");
  return tableHead;
}

String getTableRow2Col(String col1Val, String col2Val) {
  String tableRow = F("<tr><td>");
  tableRow += col1Val;
  tableRow += F("</td><td>");
  tableRow += col2Val;
  tableRow += F("</td></tr>");

  return tableRow;
}

String getTableFoot() {
  return F("</tbody></table></div></div>");
}

String getHTMLFoot() {
  return F("</body></html>");
}

String getStatusItemBare(String var, String label="") {
/*  
 *  Given a controller variable name, returns content of 
 *  label/value/unit. Used by main status page to 
 *  place values on graphic using CSS.
 */
  String result;
//  if ( var.endsWith("\0") ) var.remove(var.length(),1);  // remove c string trailing null
  int addr = findAddrByVar(var);
  fullReg reg;
  if (addr >= 0) {
    mbGetFullReg(reg, addr);
    String val = reg.value;
    if (val == "-0.00") val = "0.00";
    result += label + val + " " + mbRegUnitName[reg.unit];
  }
  return result;
}

String getStatusItemDiv(String var, String label="") {
/*  
 *  Given a controller variable name, returns a div
 *  with an id of that name, and content of 
 *  label/value/unit. Used by main status page to 
 *  place values on graphic using CSS.
 */
  String result;
  int addr = findAddrByVar(var);
  fullReg reg;
  if (addr >= 0) {
    mbGetFullReg(reg, addr);
    result += F("<div id=\"");  
    result += var;
    result += F("\">");
    result += label + reg.value + " " + mbRegUnitName[reg.unit];
    result += F("</div>");
  }
  return result;
}

String getRadioGroupHeader(String header) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-3\">");
  result += header;
  result += F("</label>");
  return result;
}

String getRadioButton(String group_name, String text, String value, boolean checked) {
  String result = F("<div class=\"col-xs-1\">");
  if (checked) {
    result += F("<label class=\"radio-inline\"><input checked type=\"radio\" name=\"");
  } else {
    result += F("<label class=\"radio-inline\"><input type=\"radio\" name=\"");
  }
  result += group_name;
  result += F("\" value=\"");
  result += value;
  result += F("\"> ");
  result += text;
  result += F("</label></div>");
  return result;
}

String getRadioGroupFooter() {
  String result = F("</div>");
  return result;
}

String getCheckBox(String checkbox_name, String value, String text, boolean checked) {
  String result = F("<div class=\"form-group\"><div class=\"col-xs-offset-3 col-xs-9\"><label class=\"checkbox-inline\">");
  if (checked) {
    result += F("<input checked type=\"checkbox\" name=\"");
  } else {
    result += F("<input type=\"checkbox\" name=\"");
  }

  result += checkbox_name;
  result += F("\" value=\"");
  result += value;
  result += F("\"> ");
  result += text;
  result += F("</label></div></div>");

  return result;
}

String getDropDownHeader(String heading, String group_name, boolean wide) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-3\">");
  result += heading;
  if (wide) {
    result += F("</label><div class=\"col-xs-8\"><select class=\"form-control\" name=\"");
  } else {
    result += F("</label><div class=\"col-xs-2\"><select class=\"form-control\" name=\"");
  }
  result += group_name;
  result += F("\">");
  return result;
}

String getDropDownOption (String value, String text, boolean checked) {
  String result = "";
  if (checked) {
    result += F("<option selected value=\"");
  } else {
    result += F("<option value=\"");
  }
  result += value;
  result += F("\">");
  result += text;
  result += F("</option>");
  return result;
}

String getDropDownFooter() {
  return F("</select></div></div>");
}

String getNumberInput(String heading, String input_name, int minVal, int maxVal, int value, boolean disabled) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"");
  result += input_name;
  result += F("\">");
  result += heading;
  result += F("</label><div class=\"col-xs-2\"><input type=\"number\" class=\"form-control\" name=\"");
  result += input_name;
  result += F("\" id=\"");
  result += input_name;
  result += F("\" min=\"");
  result += minVal;
  result += F("\" max=\"");
  result += maxVal;
  result += F("\" value=\"");
  result += value;
  result += F("\"");
  if (disabled) {
    result += F(" disabled");
  }
  result += F("></div></div>");

  return result;
}

String getNumberInputWide(String heading, String input_name, byte minVal, byte maxVal, byte value, boolean disabled) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-8\" for=\"");
  result += input_name;
  result += F("\">");
  result += heading;
  result += F("</label><div class=\"col-xs-2\"><input type=\"number\" class=\"form-control\" name=\"");
  result += input_name;
  result += F("\" id=\"");
  result += input_name;
  result += F("\" min=\"");
  result += minVal;
  result += F("\" max=\"");
  result += maxVal;
  result += F("\" value=\"");
  result += value;
  result += F("\"");
  if (disabled) {
    result += F(" disabled");
  }
  result += F("></div></div>");

  return result;
}

String getTextInput(String heading, String input_name, String value, boolean disabled) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"");
  result += input_name;
  result += F("\">");
  result += heading;
  result += F("</label><div class=\"col-xs-2\"><input type=\"text\" class=\"form-control\" name=\"");
  result += input_name;
  result += F("\" id=\"");
  result += input_name;
  result += F("\" value=\"");
  result += value;
  result += F("\"");
  if (disabled) {
    result += F(" disabled");
  }
  result += F("></div></div>");

  return result;
}

String getTextInputWide(String heading, String input_name, String value, boolean disabled) {
  String result = F("<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"");
  result += input_name;
  result += F("\">");
  result += heading;
  result += F("</label><div class=\"col-xs-8\"><input type=\"text\" class=\"form-control\" name=\"");
  result += input_name;
  result += F("\" id=\"");
  result += input_name;
  result += F("\" value=\"");
  result += value;
  result += F("\"");
  if (disabled) {
    result += F(" disabled");
  }
  result += F("></div></div>");

  return result;
}

String getSubmitButton(String buttonText) {
  String result = F("<div class=\"form-group\"><div class=\"col-xs-offset-3 col-xs-9\"><input type=\"submit\" class=\"btn btn-primary\" value=\"");
  result += buttonText;
  result += F("\"></div></div>");
  return result;
}

String getJsButton(String buttonText, String onClick) {
  debugMsgln(F("getJsButton"),4);
  String result = F("<form><input type=\"button\" value=\"");
  result += buttonText;
  result += F("\" onclick=\"");
  result += onClick + F("\"></form>");
  return result;
}

String getMIMEtype(String filename) { // file extension to the MIME type https://www.iana.org/assignments/media-types/media-types.xhtml
//  if (filename.endsWith(".src")) {
//    filename = filename.substring(0, path.lastIndexOf("."));      // not clear why?
//  } 
  if      (filename.endsWith(F(".html"))) return F("text/html");
  else if (filename.endsWith(F(".htm")))  return F("text/html");
  else if (filename.endsWith(F(".css")))  return F("text/css");
  else if (filename.endsWith(F(".js")))   return F("application/javascript");
  else if (filename.endsWith(F(".ico")))  return F("image/x-icon");  // non-standard
  else if (filename.endsWith(F(".png")))  return F("image/png");
  else if (filename.endsWith(F(".gif")))  return F("image/gif");
  else if (filename.endsWith(F(".jpg")))  return F("image/jpeg");
  else if (filename.endsWith(F(".xml")))  return F("text/xml");
  else if (filename.endsWith(F(".pdf")))  return F("application/pdf");
  else if (filename.endsWith(F(".zip")))  return F("application/zip");
  else if (filename.endsWith(F(".gz")))   return F("application/gzip");
  else if (filename.endsWith(F(".json"))) return F("application/json");
  return F("text/plain");
}
