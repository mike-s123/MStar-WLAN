
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
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}

function setRtcTime () {
	/* format time as YYYYMMDDdHHMMSS */
	var currentTime = new Date();
	var foo = currentTime.getFullYear().toString();
  	var theTime = foo.padStart(4,'0');
	foo = currentTime.getMonth()+1;
	foo = foo.toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getDate().toString();
  	theTime += foo.padStart(2,'0');
  	theTime += currentTime.getDay().toString();
	foo = currentTime.getHours().toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getMinutes().toString();
  	theTime += foo.padStart(2,'0');
	foo = currentTime.getSeconds().toString();
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

function setAging (value, vari) {
  var theUrl = "/cmd?setagingoffset=" + value + "&version=" + Math.random() ;
  var xhr = new XMLHttpRequest() ;
  document.getElementById(vari).innerHTML = value;
  xhr.open ( "GET", theUrl, false ) ;
  xhr.send() ;
}
