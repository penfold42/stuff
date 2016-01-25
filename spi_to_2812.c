/*

A bitbashed WS2801 to WS2811 protocol converter
Tested at 400,000, 600,000 bit/second SPI data from hyperion on a rpi1

                            __ __
                     RST  -|o V  |- (vcc)	+5V 
                     PB3  -|     |-  PB2	SCK (SCK)
WS2812 data          PB4  -|     |-  PB1	DO	(MISO)
ground              (gnd) -|_____|-  PB0	DI	(MOSI)


*/


#define	CPU_PRESCALE 1					// attiny13 OSC prescaler - defaults to 8 on startup
#define F_CPU 16000000/CPU_PRESCALE		// assuming the internal OSC at 16MHz

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>	
#include <avr/sfr_defs.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <math.h>
double log2(double x);


void software_spi();
void fill (uint8_t _r, uint8_t _g, uint8_t _b, int _pause);
void colourfade_strip();
void josh_sendByte( unsigned char byte );
void josh_sendBit( uint8_t bitVal );
void josh_ledsetup();
void josh_sendPixel( unsigned char r, unsigned char g , unsigned char b );
void josh_show();
void josh_showColor( unsigned char r , unsigned char g , unsigned char b );


#define brite 16

volatile char go_update_eeprom = 0;
unsigned int eeprom_update_counter = -1;

#define MAX_PIXELS 128  // Number of pixels in the string
char spi_buffer[3*MAX_PIXELS];
volatile uint16_t spi_ptr = 0;


	
void setup_timer(void){	

//	TCCR0A = 0<<WGM01 | 0<<WGM00;		// normal counter, no waveform generation
	TCCR0A = 1<<WGM01 | 0<<WGM00;		// CTC mode

//	TCCR0B = 0<<CS02 | 0<<CS01 | 1<<CS00;	// 001 clk I/O /1(No prescaling)
//	TCCR0B = 0<<CS02 | 1<<CS01 | 0<<CS00;	// 010 clk I/O /8 (From prescaler)
	TCCR0B = 0<<CS02 | 1<<CS01 | 1<<CS00;	// 011 clk I/O /64 (From prescaler) <--
//	TCCR0B = 1<<CS02 | 0<<CS01 | 0<<CS00;	// 100 clk I/O /256 (From prescaler)
//	TCCR0B = 1<<CS02 | 0<<CS01 | 1<<CS00;	// 101 clk I/O / 1024

	#define LATCH_TIMEOUT 400	// uSecs to wait for no data before displaying it
	#define OCR_VAL LATCH_TIMEOUT * 16 / 64
	OCR0A = OCR_VAL;	// 500uSec = 125 counts of 16M/64
//	OCR0A = 125;	// 500uSec = 125 counts of 16M/64
	
	#if defined (TIMSK0)
	TIMSK0 = 1<<OCIE0A; // OCIE0A: output compare A match
	#elif defined (TIMSK)
	TIMSK = 1<<OCIE0A; // OCIE0A: output compare A match
	#else
	#error timer registers not defined
	#endif 
}	

//#define DEBUG_LED
ISR(TIMER0_COMPA_vect) {
	USISR=(1<<USIOIF);
	if (spi_ptr >= 1) {			 // have we had at least 1 byte?
		if (spi_ptr%3 == 0) {	 // check for divisibility by 3, if not we know its corrupt
#ifdef DEBUG_LED
				josh_sendByte(brite);
				josh_sendByte(0);
				josh_sendByte(0);
#endif
			for (int i=0; i<spi_ptr; ) {
				josh_sendByte(spi_buffer[i++]);
				josh_sendByte(spi_buffer[i++]);
				josh_sendByte(spi_buffer[i++]);
			}
		} 
#ifdef DEBUG_LED
		if (spi_ptr%3 == 1) {
				josh_sendByte(0);
				josh_sendByte(0);
				josh_sendByte(brite);
		}
		if (spi_ptr%3 == 2) {
				josh_sendByte(0);
				josh_sendByte(brite);
				josh_sendByte(0);
		}
#endif
		spi_ptr = 0;
	}
}

	
// positive edge int on PB2
// read bits on PB0	
// led on chip pin 2 PB3
void setup_PCINT0(void) {
	PCMSK = (1<<PORTB2);	// mask to enable pcint on B2
	GIMSK = (1<<PCIE);		// enable pin change interrupts
	DDRB |= (1<<PORTB3);	// debug LED
}

ISR( PCINT0_vect) {

	if (PINB & 1<<PORTB2) {	// high now, so rising edge
		josh_sendBit( PINB&(1<<PORTB0) );

		if (PINB & (1<<PORTB0)) {	// copy data to LED
			PORTB |= (1<<PORTB3);
		} else {
			PORTB &= ~(1<<PORTB3);
		}

	}
}


 void setup_spi() {
	DDRB &= ~( (1<<PORTB2) | (1<<PORTB0) ); // USCK, DI to inputs
//	PORTB |= ( (1<<PORTB2) | (1<<PORTB0) ); // USCK, DI pullups
	USICR = (1<<USIWM0)  // SPI mode
//		|(1<<USIOIE)  // Enable interrupt
		|(1<<USICS1); // Clock is external
 
	USISR=(1<<USIOIF);	// clear any pending flag
 }


int main(void) {
	CLKPR = (1<<CLKPCE);					// enable register
	CLKPR = (uint8_t)log2(CPU_PRESCALE);	// update register

	josh_ledsetup();
	setup_spi();
	colourfade_strip();
//	setup_timer();

	software_spi();	// never returns

}



void colourfade_strip() {
	#define SLOW 4
	#define FAST 256 //32
	int r=0;  int g=0;  int b=0;
	int step = FAST;
	USISR = (1<<USIOIF);
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (r=0; r<=255; r+=step) fill( r,0,0,10 ); // black to red
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (g=0; g<=255; g+=step) fill( 255,g,0,10 ); // red to yellow
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (r=255; r>=0; r-=step) fill( r,255,0,10 ); // yellow to green
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (b=0; b<=255; b+=step) fill( 0,255,b,10 );  // green to cyan
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (g=255; g>=0; g-=step) fill( 0,g,255,10 );  // cyan to blue
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (r=0; r<=255; r+=step) fill( r,0,255,10 );  // blue to purple
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (g=0; g<=255; g+=step) fill( 255,g,255,10 );  // purple to white
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	for (g=255; g>=0; g-=(step/2)) fill( g,g,g,10 );  // white to black
	if ((USISR & (1<<USIOIF))) { step = FAST; }
	fill( 0,0,0,10 );  // force black
}

void fill (uint8_t _r, uint8_t _g, uint8_t _b, int _pause) {
	cli();  
	for( int p=0; p<MAX_PIXELS; p++ ) {
		josh_sendPixel(_r*brite/255,_g*brite/255,_b*brite/255);
	}
	sei();
	for (int p=0; p<_pause; p++){
		_delay_ms(1);
	}
}




// ************************************************************************
#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

#define RES 6000    // Width of the low gap between bits to cause a frame to latch

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives
#define CYCLES_PER_SEC (F_CPU)
#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )	// 62 @ 16M
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

#define PIXEL_PORT  PORTB  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRB   // Port of the pin the pixels are connected to
#define PIXEL_BIT   4      // Bit of the pin the pixels are connected to

void josh_sendBit( uint8_t bitVal ) {
  
    if (  bitVal ) {				// 1 bit
      
		asm volatile (
			"sbi %[port], %[bit] \n\t"				// Set the output bit
			".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			"cbi %[port], %[bit] \n\t"                              // Clear the output bit
//			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
//			"nop \n\t"
//			".endr \n\t"
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
//			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
//			"nop \n\t"
//			".endr \n\t"
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

void josh_ledsetup() {
//  bitSet( PIXEL_DDR , PIXEL_BIT );
  PIXEL_DDR |= (1<<PIXEL_BIT);
}

void josh_sendPixel( unsigned char r, unsigned char g , unsigned char b )  {  
  josh_sendByte(g);          // Neopixel wants colors in green then red then blue order
  josh_sendByte(r);
  josh_sendByte(b);
}


// Just wait long enough without sending any bits to cause the pixels to latch and display the last sent frame
void josh_show() {
	_delay_us( (RES / 1000UL) + 1);				// Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}



void software_spi() {
	DDRB |= (1<<PORTB3);	// debug LED
	while (1) {
		while ((PINB & 1<<PORTB2)) {};		// spin waiting for it to go low
		while ((PINB & 1<<PORTB2) == 0) {};		// spin waiting for it to go high

		if (  PINB&1<<PORTB0 ) {				// 1 bit
			
			asm volatile (
			"sbi %[port], %[bit] \n\t"				// Set the output bit
			".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			"cbi %[port], %[bit] \n\t"                              // Clear the output bit
//			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
//			"nop \n\t"
//			".endr \n\t"
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
//			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
//			"nop \n\t"
//			".endr \n\t"
			::
			[port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
			[bit]		"I" (PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T0H) - 2),
			[offCycles]	"I" (NS_TO_CYCLES(T0L) - 2)
			);
		}

	}
}