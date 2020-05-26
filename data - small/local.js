

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

function setWLAN ( ssid, pass, vari ) {
  var theUrl = "/cmd?setwlan=" + vari + "&ssid=" + encodeURIComponent(ssid) + "&pass" + encodeURIComponent(pass) + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById("ssid"+vari).innerHTML = ssid;
  document.getElementById("pass"+vari).innerHTML = pass;
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

function restart_ctl () {
	var theUrl = "/rest?json={\"addr\":255,\"cmd\":\"writeSingleCoil\",\"valu\":\"on\",\"pass\":\"imsure\"}" ;
	var xhr = new XMLHttpRequest() ;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
  setTimeout(location.reload(),5000);  //reload after 5 seconds
}
