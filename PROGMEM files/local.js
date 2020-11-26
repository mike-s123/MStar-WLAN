

function goBack() {              // return to previous page
  window.history.back();
}

function httpCmd ( theReq ) {
  var theUrl = "/cmd?" + theReq + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  xhr.onreadystatechange = function() {
    if ( xhr.readyState == XMLHttpRequest.DONE ) {
      resultstr.value = xhr.responseText ;
    }
  }
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function writeReg ( addr, value, vari ) {
  var theUrl = "/cmd?writereg=" + addr + "&data=" + value + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
/*  xhr.onreadystatechange = function() {
    if ( xhr.readyState == XMLHttpRequest.DONE ) {
      resultstr.value = xhr.responseText ;
      if ( resultstr.value != "err" ) {
        document.getElementById(vari).innerHTML = value;
        document.getElementById("warning").innerHTML = "(changed, controller needs reset)";
      } else {
	    document.getElementById("warning").innerHTML = "(error setting value)";
	  }
    }
  }
*/  
  document.getElementById(vari).innerHTML = value;
  document.getElementById("warning").innerHTML = "(changed, controller needs reset)";
  location.reload(); 
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setWlanSsid ( slot, ssid ) {
  var theUrl = "/cmd?setwlan=" + slot + "&ssid=" + encodeURIComponent(ssid) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("ssid"+slot).innerHTML = ssid;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setWlanPsk ( slot, psk ) {
  var theUrl = "/cmd?setwlan=" + slot + "&psk=" + encodeURIComponent(psk) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("psk"+slot).innerHTML = psk;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setApSSID ( ssid ) {
  var theUrl = "/cmd?setapssid=" + encodeURIComponent(ssid) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("apSSID").innerHTML = ssid;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setApPSK ( pass ) {
  var theUrl = "/cmd?setappsk=" + encodeURIComponent(pass) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("appsk").innerHTML = pass;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setStaHostname ( hostname ) {
  var theUrl = "/cmd?sethostname=" + encodeURIComponent(hostname) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("stahn").innerHTML = hostname;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setJSONpass ( pass ) {
  var theUrl = "/cmd?setjsonpass=" + encodeURIComponent(pass) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("jsonpass").innerHTML = pass;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setCred ( where, user, pass ) {
  var theUrl = "/cmd?setcred=" + where + "&user=" + encodeURIComponent(user) + "&pass=" + encodeURIComponent(pass) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  if ( user.length > 0 ) { document.getElementById(where + "user").innerHTML = user; } ;
  if ( pass.length > 0 ) { document.getElementById(where + "pass").innerHTML = pass; } ;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setDebugLvl ( level ) {
  if (level > 9) {level = 9} ;
  if (level < 1) {level = 1} ;
  var theUrl = "/cmd?setdebuglvl=" + encodeURIComponent(level) + "&version=" + Math.random() ;
  var theLvl = level ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("debuglvl").innerHTML = level;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setRtcTime () {
	/* format time as YYYYMMDDdHHMMSS */
	var currentTime = new Date();
	var foo = currentTime.getUTCFullYear().toString();
  	var theTime = foo.padStart(4,'0');
	foo = currentTime.getUTCMonth()+1;
	foo = foo.toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getUTCDate().toString();
  	theTime += foo.padStart(2,'0');
  	theTime += currentTime.getUTCDay().toString();
	foo = currentTime.getUTCHours().toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getUTCMinutes().toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getUTCSeconds().toString();
  	theTime += foo.padStart(2,'0');
	
  var theUrl = "/cmd?setrtc=" + theTime + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  xhr.onreadystatechange = function() {
    if ( xhr.readyState == XMLHttpRequest.DONE ) {
      resultstr.value = xhr.responseText ;
    }
  }
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  location.reload(true);
}

function setRtcTimeNTP () {
  var theUrl = "/cmd?setrtcntp=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  xhr.onreadystatechange = function() {
    if ( xhr.readyState == XMLHttpRequest.DONE ) {
      resultstr.value = xhr.responseText ;
    }
  }
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  location.reload(true);
}

function setNtpCfg () {
  var ntp_svr = document.getElementById("ntp_svr").value;
  var ntp_poll = document.getElementById("ntp_poll").value;
  var ntp_tz = encodeURIComponent(document.getElementById("ntp_tz").value) ;
  var theUrl = "/cmd?setntpcfg=true&ntp_svr=" + ntp_svr + "&ntp_poll=" + ntp_poll + "&ntp_tz=" + ntp_tz + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  location.reload(true);
}

function setAging (id) {
  var value = document.getElementById(id).value;
  var theUrl = "/cmd?setagingoffset=" + value + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  location.reload(true);
}

function restart_ctl (pass) {
	var theUrl = '/rest?json={"addr":255%2C"cmd":"writeSingleCoil"%2C"valu":"on"%2C"pass":"' ;
	theUrl += pass ;
	theUrl += '"}' ;
	var xhr = new XMLHttpRequest() ;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  setTimeout(location.reload(),5000);  //reload after 5 seconds
}
