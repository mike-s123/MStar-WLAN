

function updateRegisters () {
	const FETCH_TIMEOUT = 5000;
	let didTimeOut = false;
	var items = [{ctrlReg:"adc_vb_f_1m",label:""},
				 {ctrlReg:"adc_ib_f_1m",label:""},
				 {ctrlReg:"vb_ref",label:"Target: "},
				 {ctrlReg:"T_batt",label:""},
				 {ctrlReg:"adc_va",label:""},
				 {ctrlReg:"adc_ia",label:""},
  //               {ctrlReg:"Absolute_Pmax",label:"PMax: "},
				 {ctrlReg:"adc_il",label:""},
				 {ctrlReg:"Ahc_daily",label:""},
				 {ctrlReg:"Ahl_daily",label:""},
				 {ctrlReg:"time_ab_daily",label:"Abs: "},
				 {ctrlReg:"time_fl_daily",label:"Float: "},
				 {ctrlReg:"time_eq_daily",label:"Eq: "},
				 {ctrlReg:"Vb_min_daily",label:"Min: "},
				 {ctrlReg:"Vb_max_daily",label:"Max: "},
				 {ctrlReg:"adc_vl",label:""},
				 {ctrlReg:"Va_max_daily",label:"Max: "}
				 ];
	var power = 0;             
	var baseUri = "/status?json={\"cmd\":\"readRegs\"%2C\"registers\":";
	var regarray = "[";
	var i; for (i = 0; i < items.length; i++) {    // create the json query
		if ( i > 0 ) { regarray += "%2C"; }
		var reg = items[i].ctrlReg;
		regarray += "{\"register\":\"";
		regarray += reg ;
		regarray += "\"}";
	};
	regarray += "]}";
	var theUri = baseUri + regarray;
	new Promise(function(resolve, reject) {
		const timeout = setTimeout(function() {
			didTimeOut = true;
			reject(new Error('Request timed out'));
		}, FETCH_TIMEOUT);	
		fetch(theUri)
		.then(async function(response) {
			// Clear the timeout as cleanup
			clearTimeout(timeout);
			if(!didTimeOut) {
				console.log('fetch good! ', response);
				resolve(response);
				await response.json().then(function(obj) {        // this loops through the responses
					for (i = 0; i < obj.registers.length; i++) {
						if ( obj.registers[i].register.name == "adc_va" ) { power = obj.registers[i].register.valu };
						if ( obj.registers[i].register.name == "adc_ia" ) { power *= obj.registers[i].register.valu };
						let item = items.find(o => o.ctrlReg === obj.registers[i].register.name );
						document.getElementById(obj.registers[i].register.name).innerHTML = item.label + obj.registers[i].register.valu + " " + obj.registers[i].register.unit;
						document.getElementById(obj.registers[i].register.name + "-body").innerHTML = obj.registers[i].register.valu + " " + obj.registers[i].register.unit;
					}
					document.getElementById("Sweep_Pmax").innerHTML = power.toFixed(2) + " W" ;
					document.getElementById("Sweep_Pmax-body").innerHTML = power.toFixed(2) + " W" ;                                                    
				});
			}
		})
		.catch(function(err) {
			console.log('fetch failed! ', err);
			// Rejection already happened with setTimeout
			if(didTimeOut) return;
			// Reject with error
			reject(err);
		});
	})
	.then(function() {
		// Request success and no timeout
		console.log('good promise, no timeout! ');
	})
	.catch(function(err) {
		// Error: response error, request timeout or runtime error
		console.log('promise error! ', err);
	});
}

  
function getMaxPwr(uri) {
	const FETCH_TIMEOUT = 5000;
	let didTimeOut = false;
	new Promise(function(resolve, reject) {
		const timeout = setTimeout(function() {
			didTimeOut = true;
			reject(new Error('Request timed out'));
		}, FETCH_TIMEOUT);	

		fetch(uri)
		.then(async function(response) {
			// Clear the timeout as cleanup
			clearTimeout(timeout);
			if(!didTimeOut) {
				console.log('fetch good! ', response);
				resolve(response);
				let pwr = await response.text();
				document.getElementById("real_pmax").innerHTML = "Max: " + pwr + " W" ;
				document.getElementById("real_pmax-body").innerHTML = pwr + " W" ;
			}
		})
		.catch(function(err) {
			console.log('fetch failed! ', err); 
			// Rejection already happened with setTimeout
			if(didTimeOut) return;
			// Reject with error
			reject(err);
		});
	})
	.then(function() {
		// Request success and no timeout
		console.log('good promise, no timeout! ');
	})
	.catch(function(err) {
		// Error: response error, request timeout or runtime error
		console.log('promise error! ', err);
	});
}

function updateStatus() {
	updateRegisters();
	getMaxPwr('/status?maxp');
	setTimeout(updateStatus, 10000);  // update every 10 seconds
}

window.onload = updateStatus;
