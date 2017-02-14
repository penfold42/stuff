

var AEC = 0;
var nCAS = 0;
var nRAS = 0;

var DBUS = 0;
// D0..D11
var D = [0,0,0,0,0,0,0,0,0,0,0,0]

var RW = 1;
var nAEC = 0;
var nRESET = 0;
var Ph0 = 0;
var Ph2 = 0;
var COLORCLOCK = 0;
var DOTCLOCK = 0;
var nDMA = 1;
var nIRQ = 0;
var BA = 1;
var RDY = 0;
var nHIRAM = 1;
var nLORAM = 1;
var nCHAREN = 1;
var CAEC = 0;
var ABUS = 0;

var CPU_ADDRESS = 0xD000;
var VIC_ADDRESS = 0x0400;

var A = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
var ABUS = 0;

var nEXROM = 1;
var nGAME = 1;

var nROMH = 1;
var nROML = 1;
var nIO = 1;
var nCOLOR = 1;
var nCOLORRAM = 0;
var nSID = 1;
var nVIC = 1;
var nCASRAM = 1;
var nBASIC = 1;
var nKERNAL = 1;
var nCHAROM = 1;
var GRW = 1;

var U14_Y3 = 0;
var U14_Y2 = 0;
var VA6 = 0;
var VA7 = 0;
var VA13 = 0;
var VA12 = 0;
var nVA14 = 1;
var nVA15 = 1;

var MA0 = 0;
var MA1 = 0;
var MA2 = 0;
var MA3 = 0;
// var MA4 = 0;	// use VA12
// var MA5 = 0;	// use VA13
var MA6 = 0;
var MA7 = 0;

var CIAS = 1;
var CIA1 = 1;
var CIA2 = 1;
var IO1 = 1;
var IO2 = 1;
var nIO = 0;


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

var U26latch = [0,0,0,0,0,0,0,0]

var master_clock = 0;

var pla_rom = [];
var kernal_rom = [];
var basic_rom = [];
var chargen_rom = [];

function init_c64() {
	pla_rom = load_binary_resource("PLA.BIN");
	kernal_rom = load_binary_resource("kernal");
	basic_rom = load_binary_resource("basic");
	chargen_rom = load_binary_resource("chargen");
//	alert('init_c64');
}

var playTimer;
function play() {
	clearInterval(playTimer);
	playTimer = setInterval(step_simulation, 320);
}

function stop() {
	clearInterval(playTimer);
}

function step_simulation() {
	document.getElementById("master_clock").value = master_clock;
	update_all();
	master_clock++;
	master_clock &= 0xf;
}

function update_all() {
	update_clock_lines();
	update_lines();
	paint_page();
}

function paint_page() {
	change_CSS_signal('.DOTCLOCK', DOTCLOCK);
	change_CSS_signal('.Ph0', Ph0);
	change_CSS_signal('.Ph2', Ph2);
	change_CSS_signal('.AEC', AEC);
	change_CSS_signal('.nAEC', nAEC);
	change_CSS_signal('.nRAS', nRAS);
	change_CSS_signal('.nCAS', nCAS);

	change_CSS_chip('.U19', U19);
	change_CSS_chip('.U18', U18);
	change_CSS_chip('.U13', U13);
	change_CSS_chip('.U25', U25);
	change_CSS_chip('.U14', U14);
	change_CSS_chip('.U26', U26);
	change_CSS_chip('.U16', U16);

	change_CSS_chip('.U3', U3);
	change_CSS_chip('.U4', U4);
	change_CSS_chip('.U5', U5);
	change_CSS_chip('.U6', U6);
	change_CSS_chip('.U15a', U15a);
	change_CSS_chip('.U15b', U15b);
	change_CSS_chip('.U12', U12);

	change_CSS_signal('.U14_Y2', U14_Y2);
	change_CSS_signal('.U14_Y3', U14_Y3);
	change_CSS_signal('.VA6', VA6);
	change_CSS_signal('.VA7', VA7);
	change_CSS_signal('.nVA14', nVA14);
	change_CSS_signal('.nVA15', nVA15);

	for (var i=0; i<=15; i++) {
		change_CSS_signal('.A'+i, A[i]);
	}

	for (var i=0; i<=11; i++) {
		change_CSS_signal('.D'+i, D[i]);
	}

	change_CSS_signal('.MA0', MA0);
	change_CSS_signal('.MA1', MA1);
	change_CSS_signal('.MA2', MA2);
	change_CSS_signal('.MA3', MA3);
	change_CSS_signal('.VA12', VA12);
	change_CSS_signal('.VA13', VA13);
	change_CSS_signal('.MA6', MA6);
	change_CSS_signal('.MA7', MA7);

	change_CSS_signal('.CAEC', CAEC);
	change_CSS_signal('.RDY', RDY);
	change_CSS_signal('.nCOLORRAM', nCOLORRAM);
	change_CSS_signal('.nCOLOR', nCOLOR);
	change_CSS_signal('.nVIC', nVIC);
	change_CSS_signal('.nSID', nSID);

	change_CSS_signal('.CIAS', CIAS);
	change_CSS_signal('.CIA1', CIA1);
	change_CSS_signal('.CIA2', CIA2);
	change_CSS_signal('.IO1', IO1);
	change_CSS_signal('.IO2', IO2);
	change_CSS_signal('.nIO', nIO);

	change_CSS_signal('.nHIRAM', nHIRAM );
	change_CSS_signal('.nLORAM', nLORAM );
	change_CSS_signal('.nCHAREN', nCHAREN );

	change_CSS_signal('.nCASRAM', nCASRAM);
	change_CSS_signal('.nBASIC', nBASIC);
	change_CSS_signal('.nKERNAL', nKERNAL);
	change_CSS_signal('.nCHAROM', nCHAROM);
	change_CSS_signal('.GRW', GRW);
	change_CSS_signal('.RW', RW);
	change_CSS_signal('.nROML', nROML);
	change_CSS_signal('.nROMH', nROMH);

	change_CSS_signal('.nEXROM', nEXROM);
	change_CSS_signal('.nGAME', nGAME);

	change_CSS_signal('.nDMA', nDMA);
	change_CSS_signal('.BA', BA);
}


function update_clock_lines() {
	DOTCLOCK = (master_clock & 0x2 )/2;

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

	if (master_clock == 0) {
		CPU_ADDRESS += 0x100;
		VIC_ADDRESS += 1;
		document.getElementById("CPU_ADDRESS").value = toHex(CPU_ADDRESS);
		document.getElementById("VIC_ADDRESS").value = toHex(VIC_ADDRESS);
	}

	if (AEC == 1) {
		document.getElementById("CPU_ADDRESS").style.backgroundColor = "yellow";
		document.getElementById("VIC_ADDRESS").style.backgroundColor = "white";
	} else {
		document.getElementById("CPU_ADDRESS").style.backgroundColor = "white";
		document.getElementById("VIC_ADDRESS").style.backgroundColor = "yellow";
	}
}

function toHex(d) {
	return '$' + ("0000" + (+d).toString(16)).substr(-4);
}

function set_A_lines(addr, start, end) {
	for (var i=start; i<=end; i++) {
		if (addr & 1<<i) {
			A[i] = 1;
		} else {
			A[i] = 0;
		}
	}
	ABUS = 0;
	for (var i=0; i<=15; i++) {
		if (A[i] == 1) {
			ABUS++;
		}
	}
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
		set_A_lines(CPU_ADDRESS, 0, 15);
	} else {
		// VIC bus cycle

		set_A_lines(VIC_ADDRESS, 0, 11);
		set_A_lines(0xffff, 12, 15);		// pullup resistors

		if (nCAS == 0) {		// high byte
			MA0 = A[8];
			MA1 = A[9];
			MA2 = A[10];
			MA3 = A[11];
			VA12 = A[12];
			VA13 = A[13];
			VA6 = A[6];		// are VA6,7 valid when /CAS is low??
			VA7 = A[7];
		} else {			// low byte
			MA0 = A[0];
			MA1 = A[1];
			MA2 = A[2];
			MA3 = A[3];
			VA12 = A[4];
			VA13 = A[5];
			VA6 = A[6];
			VA7 = A[7];
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
			MA0 = A[8];
			MA1 = A[9];
			MA2 = A[10];
			MA3 = A[11];
			VA12 = A[12];
			VA13 = A[13];
			MA6 = A[14];
			MA7 = A[15];
		} else {
			MA0 = A[0];
			MA1 = A[1];
			MA2 = A[2];
			MA3 = A[3];
			VA12 = A[4];
			VA13 = A[5];
			MA6 = A[6];
			MA7 = A[7];
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
		if (nCAS == 0) {
			MA7 = (1-U14_Y3);
			MA6 = (1-U14_Y2);
		} else {
			MA7 = (1-nVA15);
			MA6 = (1-nVA14);
		}
	} else {
		U14 = false;
/*
		U14_Y3 = -1;
		U14_Y2 = -1;
		MA7 = -1;
		MA6 = -1;
*/
	}


// U26 74ls373
	if (nRAS == 0) {
		U26latch[0] = MA0;
		U26latch[1] = MA1;
		U26latch[2] = MA2;
		U26latch[3] = MA3;
		U26latch[4] = VA12;	// MA4
		U26latch[5] = VA13;	// MA5
		U26latch[6] = MA6;
		U26latch[7] = MA7;
	}
	if (AEC == 0) {
		A[0] = U26latch[0];
		A[1] = U26latch[1];
		A[2] = U26latch[2];
		A[3] = U26latch[3];
		A[4] = U26latch[4];
		A[5] = U26latch[5];
		A[6] = U26latch[6];
		A[7] = U26latch[7];
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
	if (nBASIC == 0) { U3 = 1 };
	if (nKERNAL == 0) { U4 = 1 };
	if (nCHAROM == 0) { U5 = 1 };
	if (nCOLORRAM == 0) { U6 = 1 };


// D8..D11
	if (AEC == 1 && GRW == 0) {		// CPU write cycle
		D[8] = D[0];
		D[9] = D[1];
		D[10] = D[2];
		D[11] = D[3];
		// need to write to fake color ram if we implement it
	}
	if (AEC == 1 && GRW == 1) {		// CPU read cycle
		// need to set D8..D11 from the fake color ram if we implement it
		if (nCOLORRAM == 0) {
			var val = U6_2114(1, CPU_ADDRESS, 0);
			D[8] = (val>>0) & 0x01;
			D[9] = (val>>1) & 0x01;
			D[10] = (val>>2) & 0x01;
			D[11] = (val>>3) & 0x01;
		}

		D[0] = D[8];
		D[1] = D[9];
		D[2] = D[10];
		D[3] = D[11];
	}
	if (AEC == 0 && GRW == 1) {		// VIC read cycle
		// need to set D8..D11 from the fake color ram if we implement it
		if (nCOLORRAM == 0) {
			var val = U6_2114(1, CPU_ADDRESS, 0);
			D[8] = (val>>0) & 0x01;
			D[9] = (val>>1) & 0x01;
			D[10] = (val>>2) & 0x01;
			D[11] = (val>>3) & 0x01;
		}
	}
	if (AEC == 0 && GRW == 0) {		// VIC write... should never happen
	}
}

function load_binary_resource(url) {
  var byteArray = [];
  var req = new XMLHttpRequest();
  req.open('GET', url, false);
  req.overrideMimeType('text\/plain; charset=x-user-defined');
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
A10	I14	VA13
A9	I11	RW
A8	I10	/AEC
A7	I6	A14
A6	I5	A15
A5	I4	/VA14
A4	I3	/CHAREN
A3	I2	/HIRAM
A2	I1	/LORAM
A1	I0	/CAS
A0	I15	VA12


D7	F7	/ROMH
D6	F0	/CASRAM
D5	F1	/BASIC
D4	F2	/KERNAL
D3	F3	/CHAROM
D2	F4	GR/W
D1	F5	/IO
D0	F6	/ROML


*/

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
		VA12	<< 0 
	|	nCAS	<< 1
	|	nLORAM	<< 2
	|	nHIRAM	<< 3
	| 	nCHAREN	<< 4
	|	nVA14	<< 5
	|	A[15]	<< 6
	|	A[14]	<< 7
	|	nAEC	<< 8
	|	RW	<< 9
	|	VA13	<< 10
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


function change_CSS_signal(id, checked){
	var svgDoc = svgObject.contentDocument;
	var styleElement = svgDoc.getElementById(id);
	if ( typeof styleElement === 'undefined' || styleElement === null) {
		styleElement = svgDoc.createElementNS("http://www.w3.org/2000/svg", "style");
		svgDoc.getElementById("myStyle").appendChild(styleElement);
	}

	if (checked == -1) {
		var colour = "stroke: none;";
		var width = "stroke-width:5;";
		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}"; // add whatever you need here
	}
	if (checked == 0) {
		var colour = "stroke: #282;";
		var width = "stroke-width:10;";
		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}"; // add whatever you need here
	}
	if (checked == 1) {
		var colour = "stroke: #D55;";
		var width = "stroke-width:7;";
		styleElement.id = id;
		styleElement.textContent = id + " { " + colour + width + "}"; // add whatever you need here
	}

	svgDoc.getElementById("myStyle").appendChild(styleElement);
}


function change_CSS_chip(id, checked){
	var svgDoc = svgObject.contentDocument;
	var styleElement = svgDoc.getElementById(id);
	if ( typeof styleElement === 'undefined' || styleElement === null) {
		styleElement = svgDoc.createElementNS("http://www.w3.org/2000/svg", "style");
		svgDoc.getElementById("myStyle").appendChild(styleElement);
	}

	if (checked == true) {
		var fill = "fill: #F2F;";
		var opacity = "fill-opacity:0.27;";
		styleElement.id = id;
		styleElement.textContent = id + " { " + fill + opacity + "}"; // add whatever you need here
	} else {
		var fill = "fill: #F2F;";
		var opacity = "fill-opacity:0.00;";
		styleElement.id = id;
		styleElement.textContent = id + " { " + fill + opacity + "}"; // add whatever you need here
	}

	svgDoc.getElementById("myStyle").appendChild(styleElement);
}

