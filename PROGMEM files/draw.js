
function draw() {
  document.write('<div id="controllergraph"></div>');
  fname="/controller.rrd";
  fnamed="/controllerd.rrd";

  var graph_options = {
    legend: {position:"nw",noColumns:3},
    lines: { show:true },
    yaxis: { autoscaleMargin: 0.05, tickDecimals: 2},
	grid:  { backgroundColor: "#DADADA", hoverable: true },
	tooltip: true,
	tooltipOpts: {show: true, content:"%s: %y.3", lines: true },
  };
  
  var ds_graph_options={
	'BattV':	{title: 'Batt V', 			label:'BattV',	color: "#CB4335"},	// red
	'NetA':		{title: 'Batt Net A', 		label:'NetA', 	color: "#2E86C1",yaxis:2},  // blue
	'TargetV':	{title: 'Batt Target V', 	label:'Target', color: "#F1948A"},  // red
	'BatTemp':	{title: 'Batt Temp \xB0C',	label:'BattT',	color: "#239B56"},  // green
	'ArrayV':	{title: 'Solar V', 			label:'ArrayV', color: "#ff8033"},  // orange
	'ArrayC':	{title: 'Solar A', 			label:'ArrayA', color: "#F9E79F",yaxis:2},  // yellow
	'LoadV':	{title: 'Load V', 			label:'LoadV', 	color: "#839192"},  // gray
	'LoadC':	{title: 'Load A', 			label:'LoadA', 	color: "#BFC9CA",yaxis:2},  // gray
	'MaxP':		{title: 'Solar W', 			label:'SolarW', color: "#8E44AD",yaxis:2},  // purple
    'State':	{title: 'Charge State', 	label:'State', 	color: "#ffffff"}}; // white

  var ds_graph_optionsd={
	'MinBattV':	{title: 'Min BattV', 	label:'MinBattV', color: "#239B56"},  // green
	'MaxBattV':	{title: 'Max BattV', 	label:'MaxBattV', color: "#CB4335"},  // red
	'ChargeAh':	{title: 'Charge Ah', 	label:'ChargeAh', color: "#F9E79F",yaxis:2},  // yellow
	'LoadAh':	{title: 'Load Ah', 		label:'LoadAh', color: "#839192",yaxis:2},  // gray
	'MaxArrV':	{title: 'Max SolarV', 	label:'MaxArrV',color: "#ff8033"},  // orange
	'AbsorpT':	{title: 'Absorp time', 	label:'AbsorpT',color: "#c0c0c0"},  // gray
	'EqT':		{title: 'Eq time', 		label:'EqT', 	color: "#ffffff"},  // white
    'FloatT':	{title: 'Float time', 	label:'FloatT', color: "#f0f0f0"},  // gray
	'MaxPD':	{title: 'Max power', 	label:'MaxPD', color: "#8E44AD",yaxis:2}}; // purple

  var rrdflot_defaults={
	use_checked_DSs: true,				//Use the list checked_DSs below.
    checked_DSs: ['BattV','NetA'],	//List of elements to be checked by default when graph is loaded. 
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