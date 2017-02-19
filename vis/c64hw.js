


var svgDoc;

var AEC = 0;
var nAEC = 0;
var CAEC = 0;

var nCAS = 0;
var nRAS = 0;
var nCASRAM = 1;

// D0..D11
var D = [0,0,0,0,0,0,0,0,0,0,0,0]
var DBUS = 0;
var DATA = 0;

var RW = 1;
var nIRQ = 1;
var nRESET = 1;

var COLORCLOCK = 0;
var DOTCLOCK = 0;
var Ph0 = 0;
var Ph2 = 0;

var nDMA = 1;
var BA = 1;
var RDY = 0;

var nHIRAM = 1;
var nLORAM = 1;
var nCHAREN = 1;


var CPU_ADDRESS = 0xD000;
var VIC_ADDRESS = 0x0400;
var BUS_ADDRESS = 0;

var A = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
var ABUS_colour = 0;	// 0..99

var MA = [0,0,0,0,0,0,0,0]


var nEXROM = 1;
var nGAME = 1;

var nROMH = 1;
var nROML = 1;
var nIO = 1;
var nCOLOR = 1;
var nCOLORRAM = 0;
var nSID = 1;
var nVIC = 1;
var nBASIC = 1;
var nKERNAL = 1;
var nCHAROM = 1;
var GRW = 1;

var U14_Y3 = 0;
var U14_Y2 = 0;
var VA6 = 0;
var VA7 = 0;
var nVA14 = 1;
var nVA15 = 1;

var CIAS = 1;
var CIA1 = 1;
var CIA2 = 1;
var IO1 = 1;
var IO2 = 1;
var nIO = 0;


var CSS_shapes = [ "U19", "U12", "U26", "U6", "U3", "U4", "U5", "U17", "U18", "U14", "U15a", "U15b", "U13", "U25", "U16" ];

var U3 = 0;
var U4 = 0;
var U5 = 0;
var U6 = 0;
var U15a = 0;
var U15b = 0;
var U12 = 0;

var U14 = 0;
var U19 = 0;
var U13 = 0;
var U25 = 0;
var U26 = 0;
var U16 = 0;
var U17 = 1;

// list of signals to CSS update
var CSS_signals = [ 
	"AEC", "nVA14", "nCAS", "nRAS", "DBUS", "RW", "nAEC", "nRESET", "Ph2",
	"COLORCLOCK", "DOTCLOCK", "nDMA", "nIRQ", "BA", "RDY", "nHIRAM", "nLORAM", "nCHAREN", "CAEC",
	"nEXROM", "nGAME", "nROMH", "nROML", "nIO", "nCOLOR", "nSID",
	"nVIC", "nVA15", "nCASRAM", "nBASIC", "nKERNAL", "nCHAROM", "GRW", "U14_Y3", "U14_Y2", 
	"Ph0", "IO2", "CIA1", "CIA2", "IO1", "CIAS", "VA7", "VA6", "nCOLORRAM"
];



var U26latch = [0,0,0,0,0,0,0,0];

var last_master_clock = 15;
var master_clock = 0;

var pla_rom = [];
var kernal_rom = [];
var basic_rom = [];
var chargen_rom = [];

function init_c64() {
	svgDoc = svgObject2.contentDocument;

	pla_rom = load_binary_resource("PLA.BIN");
	kernal_rom = load_binary_resource("kernal");
	basic_rom = load_binary_resource("basic");
	chargen_rom = load_binary_resource("chargen");

}

var playTimer;
function play() {
	clearInterval(playTimer);
	playTimer = setInterval(step_simulation, 320);
//	playTimer = setInterval(step_simulation, 0);
}

function stop() {
	clearInterval(playTimer);
}

function step_simulation(skip) {
        if ( typeof skip === 'undefined' || skip === null) {
		skip = 1;
	}
	master_clock += skip;
	master_clock &= 0xf;
	document.getElementById("master_clock").value = master_clock;
	update_all();
}

function update_all() {
	clear_lines();
	update_clock_lines();
	update_lines();
	paint_page();
}

// Pre clear most of the lines so they "float" if not explicitly set
function clear_lines() {
	U14_Y3 = -1;
	U14_Y2 = -1;

	for (var i=0; i<=11; i++) {
		D[i] = -1;
	}
	for (var i=0; i<=15; i++) {
		A[i] = -1;
	}
}



// This does all the CSS stylesheet updates
function paint_page() {

// mostly chip outlines
	for (var i in CSS_shapes) {
		change_CSS_chip('.'+CSS_shapes[i], window[CSS_shapes[i]]) ;
	}

// mostly lots of individual signals
	for (var i in CSS_signals) {
		change_CSS_signal('.'+CSS_signals[i], window[CSS_signals[i]]) ;
	}

// address lines and address bus
	var any_floating = false;
	for (var i=0; i<=15; i++) {
		change_CSS_signal('.A'+i, A[i]);
		if (A[i] == -1) { any_floating = true; }
	}
	var red = parseInt(ABUS_colour*191/99);	// colours 00-c0
	var green = 191-red;

	red = ("00" + (+red).toString(16)).substr(-2);
	green = ("00" + (+green).toString(16)).substr(-2);
	var blue = ("00");
	if (any_floating) {
		change_CSS_bus('.ABUS', -1);
	} else {
		change_CSS_bus('.ABUS', red + green + blue);
	}


// multiplexed address lines 
	for (var i=0; i<=7; i++) {
		change_CSS_signal('.MA'+i, MA[i]);
	}

// data lines and data bus
	var any_floating = false;
	for (var i=0; i<=11; i++) {
		change_CSS_signal('.D'+i, D[i]);
		if (D[i] == -1) { any_floating = true; }
	}
	var red = ("00" + (+DATA).toString(16)).substr(-2);
	var green = ("00" + (+(255-DATA)).toString(16)).substr(-2);
	var blue = ("FF");
	if (any_floating) {
		change_CSS_bus('.DBUS', -1);
	} else {
		change_CSS_bus('.DBUS', red + green + blue);
	}

// update text fields
	document.getElementById("CPU_ADDRESS").value = toHex(CPU_ADDRESS, 4);
	document.getElementById("VIC_ADDRESS").value = toHex(VIC_ADDRESS, 4);
	document.getElementById("BUS_ADDRESS").value = toHex(BUS_ADDRESS, 4);
	if (AEC == 1) {

		document.getElementById("CPU_ADDRESS").style.backgroundColor = "yellow";
		document.getElementById("VIC_ADDRESS").style.backgroundColor = "white";
	} else {
		document.getElementById("CPU_ADDRESS").style.backgroundColor = "white";
		document.getElementById("VIC_ADDRESS").style.backgroundColor = "yellow";
	}


// update all the other styles in one hit


}


function update_clock_lines() {
	DOTCLOCK = (master_clock & 0x1 );

	if (master_clock <= 7) {
		Ph0 = 0;
		Ph2 = 0;
		AEC = 0;
		U19 = 1;
	} else {
		Ph0 = 1;
		Ph2 = 1;
		AEC = 1;
		U19 = 0;
	}

	if (master_clock%8 <= 0) {
		nRAS = 1;
	} else {
		nRAS = 0;
	}

	if (master_clock%8 <= 3) {
		nCAS = 1;
	} else {
		nCAS = 0;
	}

	if (master_clock == 0 && last_master_clock == 15) {
		CPU_ADDRESS += 0x100;
		VIC_ADDRESS += 1;
	}
	last_master_clock = master_clock;

}

function toHex(number, digits) {
	return '$' + ("0000" + (+number).toString(16)).substr(-digits);
}

function set_A_lines(addr, start, end) {
	for (var i=start; i<=end; i++) {
		if (addr & 1<<i) {
			A[i] = 1;
		} else {
			A[i] = 0;
		}
	}
	ABUS_colour = 0;
	for (var i=0; i<=15; i++) {
		if (A[i] == 1) {
			ABUS_colour++;
		}
	}
	ABUS_colour *= 99/16;	// 0..99
}

function update_lines() {

// U8 7406
	if (AEC == 0) {		// VIC bus cycle
		nAEC = 1;
	} else {		// CPU bus cycle
		nAEC = 0;
	}


// Address bus lines
	if (AEC == 1) {		
		// CPU bus cycle
		BUS_ADDRESS = CPU_ADDRESS;
		set_A_lines(BUS_ADDRESS, 0, 15);
	} else {
		// VIC bus cycle
		BUS_ADDRESS = VIC_ADDRESS;
		BUS_ADDRESS |= 0xf000		// pullup resistors
		set_A_lines(BUS_ADDRESS, 0, 15);

//		if (nCAS == 0) {		// high byte for /CAS
		if (master_clock >= 2) {		// high byte for /CAS
			for (var i=0; i<=5; i++) {
				MA[i]	= (VIC_ADDRESS>>(i+8)) & 0x01;
			}
//			MA[0]	= (VIC_ADDRESS>>8) & 0x01;
//			MA[1]	= (VIC_ADDRESS>>9) & 0x01;
//			MA[2]	= (VIC_ADDRESS>>10) & 0x01;
//			MA[3]	= (VIC_ADDRESS>>11) & 0x01;
//			MA[4]	= (VIC_ADDRESS>>12) & 0x01;
//			MA[5]	= (VIC_ADDRESS>>13) & 0x01;
			VA6	= (VIC_ADDRESS>>14) & 0x01;
			VA7	= (VIC_ADDRESS>>15) & 0x01;	 // are VA6,7 valid when /CAS is low??
		}

		if (master_clock <= 1) {			// low byte for /RAS
			for (var i=0; i<=5; i++) {
				MA[i]	= (VIC_ADDRESS>>i) & 0x01;
			}
//			MA[0]	= (VIC_ADDRESS>>0) & 0x01;
//			MA[1]	= (VIC_ADDRESS>>1) & 0x01;
//			MA[2]	= (VIC_ADDRESS>>2) & 0x01;
//			MA[3]	= (VIC_ADDRESS>>3) & 0x01;
//			MA[4]	= (VIC_ADDRESS>>4) & 0x01;
//			MA[5]	= (VIC_ADDRESS>>5) & 0x01;
			VA6	= (VIC_ADDRESS>>6) & 0x01;
			VA7	= (VIC_ADDRESS>>7) & 0x01;
		}
	}

// U16 4066 near color ram
	if (AEC == 1) {
		U16 = 1;
	} else {
		U16 = 0;
	}

// U13/U25 74LS257
	if (nAEC == 0) {
		U25 = true;
		U13 = true;
		if (nCAS == 0) {
			for (var i=0; i<=7; i++) {
				MA[i] = A[i+8];
			}
		} else {
			for (var i=0; i<=7; i++) {
				MA[i] = A[i];
			}
		}
	} else {
		U25 = false;
		U13 = false;
	}

// U14 74ls258
	if (AEC == 0) {
		U14 = true;
		U14_Y3 = (1-VA7);
		U14_Y2 = (1-VA6);
		if (nCAS == 1) {
			MA[7] = (1-U14_Y3);
			MA[6] = (1-U14_Y2);
		} else {
			MA[7] = (1-nVA15);
			MA[6] = (1-nVA14);
		}
	} else {
		U14 = false;
	}


// U26 74ls373
// we model 2 parts - the internal latch and then the output enable
	if (nRAS == 1) {
		for (var i=0; i<=7; i++) {
			U26latch[i] = MA[i];
		}
	}
	if (AEC == 0) {
		for (var i=0; i<=7; i++) {
			A[i] = U26latch[i];
		}
		U26 = true;
	} else {
		U26 = false;
	}

// lookup the PLA rom table array
	set_pla_output( pla_rom[build_pla_addr()] ) ;

// U15 74ls139 IO decoder
	U15a = U15b = false;
	nVIC = nSID = nCOLOR = CIAS = 1;
	CIA1 = CIA2 = IO1 = IO2 = 1;

	if (nIO == 0) {		// $Dxxx
		U15a = true;
		var tmp = A[11]*2 + A[10];
		if (tmp==0) { nVIC = 0; }	// $D0..D3
		if (tmp==1) { nSID = 0; }	// $D4..D7
		if (tmp==2) { nCOLOR = 0; }	// $D8..DB
		if (tmp==3) { CIAS = 0; }	// $DC..DF
	}
	if (CIAS == 0) {
		U15b = true;
		var tmp = A[9]*2 + A[8];
		if (tmp==0) { CIA1 = 0; }	// $DC
		if (tmp==1) { CIA2 = 0; }	// $DD
		if (tmp==2) { IO1 = 0; }	// $DE
		if (tmp==3) { IO2 = 0; }	// $DF
	}


// random AND gates... U27
	CAEC = AEC & nDMA;
	RDY = BA & nDMA;
	nCOLORRAM = AEC & nCOLOR;

// light up the chips...
	U19 = U18 = U12 = U3 = U4 = U5 = U6 = 0;
	if (nVIC == 0) { U19 = 1 };
	if (nSID == 0) { U18 = 1 };
	if (nCASRAM == 0) { U12 = 1 };

// respond to reads from chips 
// but delay setting the data bus until master_clock 6,7,14,15
// this simulates an access time of ~180nS
	if (RW == 1 && master_clock%8 >= 6) {
		if (nBASIC == 0) {
			U3 = 1;
			set_data_lines( basic_rom[BUS_ADDRESS & 0x1fff] ) ;
		}
		if (nKERNAL == 0) {
			U4 = 1;
			set_data_lines( kernal_rom[BUS_ADDRESS & 0x1fff] ) ;
		}
		if (nCHAROM == 0) {
			U5 = 1
			set_data_lines( chargen_rom[BUS_ADDRESS & 0x0fff] ) ;
		}
	}

	if (nCOLORRAM == 0) {
		U6 = 1
		if (GRW == 1) {
			var val = U6_2114(1, BUS_ADDRESS & 0x03ff, 0);
			D[8] = (val>>0) & 0x01;
			D[9] = (val>>1) & 0x01;
			D[10] = (val>>2) & 0x01;
			D[11] = (val>>3) & 0x01;
		}
	};


// D8..D11 
// U16 4066 near color ram
	if (AEC == 1 && GRW == 0) {		// CPU write cycle
		D[8] = D[0];
		D[9] = D[1];
		D[10] = D[2];
		D[11] = D[3];
		// need to write to fake color ram if we implement it
	}
	if (AEC == 1 && GRW == 1) {		// CPU read cycle
		D[0] = D[8];
		D[1] = D[9];
		D[2] = D[10];
		D[3] = D[11];
	}
	if (AEC == 0 && GRW == 1) {		// VIC read cycle
	}
	if (AEC == 0 && GRW == 0) {		// VIC write... should never happen
	}

}

function load_binary_resource(url) {
	var byteArray = [];
	var req = new XMLHttpRequest();
	req.open('GET', url, false);
	if (req.overrideMimeType) {
		req.overrideMimeType('text\/plain; charset=x-user-defined');
	}
	req.send(null);
	if (req.status != 200) return byteArray;
		for (var i = 0; i < req.responseText.length; ++i) {
		byteArray.push(req.responseText.charCodeAt(i) & 0xff)
	}
	return byteArray;
}

/* http://personalpages.tds.net/~rcarlsen/cbm/c64/eprompla/readpla.jpg

EPROM	PLA	FUNCTION
A15	I13	/GAME
A14	I8	A12
A13	I9	BA
A12	I7	A13
A11	I12	/EXROM
A10	I14	VA13	// == MA5
A9	I11	RW
A8	I10	/AEC
A7	I6	A14
A6	I5	A15
A5	I4	/VA14
A4	I3	/CHAREN
A3	I2	/HIRAM
A2	I1	/LORAM
A1	I0	/CAS
A0	I15	VA12	// == MA4


D7	F7	/ROMH
D6	F0	/CASRAM
D5	F1	/BASIC
D4	F2	/KERNAL
D3	F3	/CHAROM
D2	F4	GR/W
D1	F5	/IO
D0	F6	/ROML


*/

function set_data_lines(data) {
	DATA	= data;

	for (var i=0; i<=7; i++) {
		D[i]	= (data>>i) & 0x01;
	}
	document.getElementById("DATA").value = toHex(data, 2);
}

function set_pla_output(data) {
	nROML	= (data>>0) & 0x01;
	nIO	= (data>>1) & 0x01;
	GRW	= (data>>2) & 0x01;
	nCHAROM	= (data>>3) & 0x01;
	nKERNAL	= (data>>4) & 0x01;
	nBASIC	= (data>>5) & 0x01;
	nCASRAM	= (data>>6) & 0x01;
	nROMH	= (data>>7) & 0x01;
}

function build_pla_addr() {
	var result =  (
		MA[4]	<< 0 
	|	nCAS	<< 1
	|	nLORAM	<< 2
	|	nHIRAM	<< 3
	| 	nCHAREN	<< 4
	|	nVA14	<< 5
	|	A[15]	<< 6
	|	A[14]	<< 7
	|	nAEC	<< 8
	|	RW	<< 9
	|	MA[5]	<< 10
	|	nEXROM	<< 11
	|	A[13]	<< 12
	|	BA	<< 13
	|	A[12]	<< 14
	|	nGAME	<< 15
	);
//	alert('build_pla_addr: ' + result);
	return(result);
}

function set_addr(id) {
	var val = document.getElementById(id).value;
        var res = val.match( /^\$?([0-9A-Fa-f]{1,4})$/ ) ;

	if (id == 'CPU_ADDRESS' || id == 'VIC_ADDRESS' ) {
		if ( res ) {
//			alert ("set_var got id: " + id + " result: " + res[1] );
			window[id] = parseInt('0x' + res[1]);
		} else {
			alert ("invalid address : " + val );
			document.getElementById(id).value = '$' + window[id].toString(16);
		}
	}
	update_all();
}

function U6_2114 (rw,addr,data) {
	if (rw == 1) {	// read cycle
		return Math.random() * (16 - 0) + 0;
	}
}


function set_signal(id, checked) {
	var name = document.getElementById(id).name;
	var val = document.getElementById(id).value;

        if ( typeof window[name] === 'undefined' || window[name] === null) {
		alert ('set_sig got id: ' + id + ' is invalid');
	} else {
		if (checked) {
			window[name] = 1;
		} else {
			window[name] = 0;
		}
	}
	update_all();
}


function change_CSS_signal(id, state){

	var styleElement = svgDoc.getElementById(id);
	if ( typeof styleElement === 'undefined' || styleElement === null) {
		styleElement = svgDoc.createElementNS("http://www.w3.org/2000/svg", "style");
		styleElement.id = id;
		svgDoc.getElementById("myStyle").appendChild(styleElement);
	}

	if (state == -1) {
		var colour = "stroke: none;";
		var width = "stroke-width:5;";
//		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}";
	}
	if (state == 0) {
		var colour = "stroke: #0C0;";	// 282
		var width = "stroke-width:6;";
//		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}";
	}
	if (state == 1) {
		var colour = "stroke: #C00;";	// D55
		var width = "stroke-width:6;";
//		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}";
	}

//	svgDoc.getElementById("myStyle").appendChild(styleElement);
}


function change_CSS_chip(id, state){

	var styleElement = svgDoc.getElementById(id);
	if ( typeof styleElement === 'undefined' || styleElement === null) {
		styleElement = svgDoc.createElementNS("http://www.w3.org/2000/svg", "style");
		styleElement.id = id;
		svgDoc.getElementById("myStyle").appendChild(styleElement);
	}

	if (state == true) {
		var fill = "fill: #F2F;";
		var opacity = "fill-opacity:0.27;";
//		styleElement.id = id;
		styleElement.textContent = id + " { " + fill + opacity + "}";
	} else {
		var fill = "fill: #F2F;";
		var opacity = "fill-opacity:0.00;";
//		styleElement.id = id;
		styleElement.textContent = id + " { " + fill + opacity + "}";
	}

	svgDoc.getElementById("myStyle").appendChild(styleElement);
}

function change_CSS_bus(id, colour_str){

	var styleElement = svgDoc.getElementById(id);
	if ( typeof styleElement === 'undefined' || styleElement === null) {
		styleElement = svgDoc.createElementNS("http://www.w3.org/2000/svg", "style");
		styleElement.id = id;
		svgDoc.getElementById("myStyle").appendChild(styleElement);
	}

	var colour = "stroke: #" + colour_str + ";";
	var width = "stroke-width:10;";
	if (colour_str == -1) {
		colour = "stroke: none;";
		width = "stroke-width:10;";
	}

//	styleElement.id = id;
	styleElement.textContent = id + " { " + colour + width + "}";

//	svgDoc.getElementById("myStyle").appendChild(styleElement);
}

