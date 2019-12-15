/*

A bitbashed WS2811 led strip tester

                            __ __
                     RST  -|o V  |- (vcc)	+5V 
/freeze              PB3  -|     |-  PB2	/full brightness
WS2812 data          PB4  -|     |-  PB1	/all white
ground              (gnd) -|_____|-  PB0	/slower speed

write lfuse 0 0xe2

*/


#if defined (__AVR_ATtiny13__)
	#define	CPU_PRESCALE 1		
//	#define F_CPU 9600000/CPU_PRESCALE		// assuming the internal OSC at 8MHz
#else
	#error unknown platform
#endif


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>	
double log2(double x);


void fill (uint8_t _r, uint8_t _g, uint8_t _b );
void colourfade_strip(void);

void josh_sendByte( unsigned char byte );
void josh_sendBit( uint8_t bitVal );

uint8_t dimness = 16;
uint8_t speed=16;

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives
#define CYCLES_PER_SEC (F_CPU)
#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )	// 62 @ 16M
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

#define NS_kHz_TO_CYCLES(n,k) ( 1L * n * k /  1000000L  )

#if defined (__AVR_ATtiny13__) || defined (__AVR_ATtiny85__) 
	#define PIXEL_PORT  PORTB  // Port of the pin the pixels are connected to
	#define PIXEL_DDR   DDRB   // Port of the pin the pixels are connected to
	#define PIXEL_BIT   4      // Bit of the pin the pixels are connected to

	#define FREEZ_MASK   1<<3
	#define BRITE_MASK   1<<2
	#define WHITE_MASK   1<<1
	#define SPEED_MASK   1<<0
	#define INPUTS_MASK (FREEZ_MASK | BRITE_MASK | SPEED_MASK | WHITE_MASK)
#else
	#error unknown platform
#endif



#define MAX_PIXELS 1000 // Number of pixels in the string - only the startup effect cares about this.
#if MAX_PIXELS > 64000
	#error MAX_PIXELS must be < 64000
#endif


int main(void) {
	CLKPR = (1<<CLKPCE);					// enable register
	CLKPR = (uint8_t)log2(CPU_PRESCALE);	// update register

	cli();		// no interrupts

        PIXEL_DDR |= (1<<PIXEL_BIT);
	PIXEL_DDR &= ~(INPUTS_MASK); // inputs
	PIXEL_PORT |= (INPUTS_MASK); // with pullups

	while (1)
		colourfade_strip();	// welcome my prettys
}



void check_inputs(void) {

	if (PINB & SPEED_MASK)
		speed=16;
	else
		speed=1;

	do {
		if (PINB & BRITE_MASK)
			dimness = 16;
		else
			dimness = 1;

		if (!(PINB & WHITE_MASK))
			fill( 255,255,255 ); // whiter shade of pale

	} while (!(PINB & WHITE_MASK));

	while (!(PINB & FREEZ_MASK));	// and do nothing while held
}

void colourfade_strip(void) {
	int16_t x=0;

	for (x=0; x<=255; x+=speed) {
		fill( x,0,0 ); // black to red
		check_inputs();
	}
	for (x=0; x<=255; x+=speed) {
		fill( 255,x,0 ); // red to yellow
		check_inputs();
	}
	for (x=255; x>=0; x-=speed) {
		fill( x,255,0 ); // yellow to green
		check_inputs();
	}
	for (x=0; x<=255; x+=speed) {
		fill( 0,255,x );  // green to cyan
		check_inputs();
	}
	for (x=255; x>=0; x-=speed) {
		fill( 0,x,255 );  // cyan to blue
		check_inputs();
	}
	for (x=0; x<=255; x+=speed) {
		fill( x,0,255 );  // blue to purple
		check_inputs();
	}
	for (x=0; x<=255; x+=speed) {
		fill( 255,x,255 );  // purple to white
		check_inputs();
	}
	for (x=255; x>=0; x-=speed) {
		fill( x,x,x );  // white to black
		check_inputs();
	}

	fill( 0,0,0 );  // force black
	check_inputs();
}

void fill (uint8_t r, uint8_t g, uint8_t b ) {
	uint8_t _r=r/dimness;
	uint8_t _g=g/dimness;
	uint8_t _b=b/dimness;
	for( uint16_t p=0; p<MAX_PIXELS; p++ ) {
		josh_sendByte(_g);
		josh_sendByte(_r);
		josh_sendByte(_b);
	}
	_delay_us(5000);	// end of frame latch time
}


// ************************************************************************
#undef orig

#ifdef orig
// these didnt work well at 8MHz clock
#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns
#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns
#else
#define T1H  700    // Width of a 1 bit in ns
#define T1L  300    // Width of a 1 bit in ns
#define T0H  350    // Width of a 0 bit in ns
#define T0L  300    // Width of a 0 bit in ns
#endif

#define RES 6000    // Width of the low gap between bits to cause a frame to latch


void josh_sendBit( uint8_t bitVal ) {
  
    if (  bitVal ) {				// 1 bit
      
		asm volatile (
			"sbi %[port], %[bit] \n\t"				// Set the output bit
			".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			"cbi %[port], %[bit] \n\t"                              // Clear the output bit
			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			::
			[port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
			[bit]		"I" (PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T1H) - 2),		// 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
			[offCycles] 	"I" (NS_TO_CYCLES(T1L) - 2)			// Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness

		);
                                  
    } else {					// 0 bit

		// **************************************************************************
		// This line is really the only tight goldilocks timing in the whole program!
		// **************************************************************************

		asm volatile (
			"sbi %[port], %[bit] \n\t"				// Set the output bit
			".rept %[onCycles] \n\t"				// Now timing actually matters. The 0-bit must be long enough to be detected but not too long or it will be a 1-bit
			"nop \n\t"                                              // Execute NOPs to delay exactly the specified number of cycles
			".endr \n\t"
			"cbi %[port], %[bit] \n\t"                              // Clear the output bit
			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			::
			[port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
			[bit]		"I" (PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T0H) - 2),
			[offCycles]	"I" (NS_TO_CYCLES(T0L) - 2)

		);
      
    }
    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
    // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
    // This has thenice side effect of avoid glitches on very long strings becuase 
}  

  
void josh_sendByte( unsigned char byte ) {
    for( unsigned char bit = 0 ; bit < 8 ; bit++ ) {
      josh_sendBit( byte & (1<<7) );                // Neopixel wants bit in highest-to-lowest order
                                                     // so send highest bit (bit #7 in an 8-bit byte since they start at 0)
      byte <<= 1;                                    // and then shift left so bit 6 moves into 7, 5 moves into 6, etc
    }           
} 



/*
Symbol 	Parameter 					Min 	Typ 	Max 	Units
T0H 	0 code ,high voltage time 	200 	350 	500 	ns
T1H 	1 code ,high voltage time 	550 	700 	5,500 	ns

TLD 	data, low voltage time 		450 	600 	5,000 	ns
TLL 	latch, low voltage time 	6,000 			ns
*/



