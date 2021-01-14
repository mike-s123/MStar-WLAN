
function draw() {
  document.write('<div id="controllergraph"></div>');
  fname="/controller.rrd";
  fnamed="/controllerd.rrd";

  var graph_options = {
    legend: {position:"nw",noColumns:3},
    lines: { show:true },
    yaxis: { autoscaleMargin: 0.20},
	grid:  { backgroundColor: "#DADADA"},
  };
  
  var ds_graph_options={
	'BattV':	{title: 'Battery voltage', 			label:'BattV',	color: "#CB4335"},	// red
	'NetA':		{title: 'Battery Net Amps', 		label:'NetA', 	color: "#2E86C1",yaxis:2},  // blue
	'TargetV':	{title: 'Battery Target Voltage', 	label:'Target', color: "#F1948A"},  // red
	'BatTemp':	{title: 'Battery Temperature \xB0C',label:'BattT',	color: "#239B56"},  // green
	'ArrayV':	{title: 'Solar Array Voltage', 		label:'ArrayV', color: "#F1C40F"},  // yellow
	'ArrayC':	{title: 'Solar Array Amps', 		label:'ArrayA', color: "#F9E79F",yaxis:2},  // yellow
	'LoadV':	{title: 'Load Voltage', 			label:'LoadV', 	color: "#839192"},  // gray
	'LoadC':	{title: 'Load Amps', 				label:'LoadA', 	color: "#BFC9CA",yaxis:2},  // gray
	'MaxP':		{title: 'Maximum Solar Power', 		label:'MaxP', 	color: "#8E44AD"},  // purple
    'State':	{title: 'Charging State', 			label:'State', 	color: "#ffffff"}}; // white

  var ds_graph_optionsd={
	'MinBattV':	{title: 'Min BattV', 	label:'MinBattV', color: "#239B56"},  // green
	'MaxBattV':	{title: 'Max BattV', 	label:'MaxBattV', color: "#F1C40F"},  // yellow
	'ChargeAh':	{title: 'Charge Ah', 	label:'ChargeAh', color: "#F9E79F"},  // yellow
	'LoadAh':	{title: 'Load Ah', 		label:'LoadAh', color: "#839192"},  // gray
	'MaxArrV':	{title: 'Max ArrayV', 	label:'MaxArrV',color: "#BFC9CA"},  // gray
	'AbsorpT':	{title: 'Absorp time', 	label:'AbsorpT',color: "#8E44AD"},  // purple
	'EqT':		{title: 'Eq time', 		label:'EqT', 	color: "#8E44AD"},  // purple
    'FloatT':	{title: 'Float time', 	label:'FloatT', color: "#ffffff"},  // white
	'MaxPD':	{title: 'Max power daily', 	label:'MaxPD', color: "#000000"}}; // black

  var rrdflot_defaults={
	use_checked_DSs: true,				//Use the list checked_DSs below.
    checked_DSs: ['BattV','TargetV'],	//List of elements to be checked by default when graph is loaded. 
	rra: 0,
    graph_height:	"300px",	//Height of main graph. 
    graph_width:	"800px",	//Width of main graph.
    scale_height:	"200px",	//Height of small scaler graph.
    scale_width:	"550px",	//Width of small scaler graph.
	timezone: -5 };				//Timezone to plot. Options are -11 through +12

  var rrdflot_defaultsd={
	use_checked_DSs: true,    //Use the list checked_DSs below.
    checked_DSs: ['MinBattV','MaxBattV'],   //List of elements to be checked by default when graph is loaded. 
	rra: 0,
    graph_height:	"300px",	//Height of main graph. 
    graph_width:	"800px",	//Width of main graph.
    scale_height:	"200px",	//Height of small scaler graph.
    scale_width:	"550px",	//Width of small scaler graph.
	timezone: -5 };				//Timezone to plot. Options are -11 through +12

  // the rrdFlotAsync object creates and handles the graph
  // rrdFlotAsync(html_id,url,file_options,graph_options,ds_graph_options,rrdflot_defaults,ds_op_list,rrd_op_list,customization_callback)
  var f=new rrdFlotAsync("controllergraph",fname,null,graph_options,ds_graph_options,rrdflot_defaults,null,null);
  var fd=new rrdFlotAsync("controllerdgraph",fnamed,null,graph_options,ds_graph_optionsd,rrdflot_defaultsd,null,null);  
}