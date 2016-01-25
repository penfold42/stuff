/*
Take spi data intended for ws2801, buffer it, spit out ws2812 protocol

speed is important if you are using hyperion so the SPI data is sent out 
using the code at http://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
This is on PORTD.4/arduino pint 4

Also uses the UART to control a second string of LEDs (max 4 at the moment)
This just uses the adafruit neopixel library
This is on arduino pin 9
*/

#define ENABLE_UART
#define BAUDRATE   9600  // Serial port speed, 
//#define BAUDRATE   115200  // Serial port speed, 

#define POLLED_MODE   // no interrupts, poll everying
                      // lots of arduino core stuff will fail tho

//#undef POLLED_MODE  // use interrupts

#include "Adafruit_NeoPixel.h"
#define ADA_DATAPIN    9        // Datapin
#define ADA_LEDCOUNT   4       //  Number of LEDs used
#define ADA_BRIGHTNESS 50
Adafruit_NeoPixel strip = Adafruit_NeoPixel(ADA_LEDCOUNT, ADA_DATAPIN, NEO_GRB + NEO_KHZ800);

// DANGER! if you change any of the IO pins, you must update this too!
byte spare_pins[] = {0,1,2,3,4,5,6,7,8,9,A0,A1,A2,A3,A4,A5};

char ada_buffer[ADA_LEDCOUNT*3+5];  // "LED" + RGB for each LED + "\n" 
byte ada_ptr;
byte update_ada;

#include <SPI.h>

#define MAX_PIXELS 300  // Number of pixels in the string
#define Josh2812_PIXEL_PORT  PORTD  // Port of the pin the pixels are connected to
#define Josh2812_PIXEL_DDR   DDRD   // Port of the pin the pixels are connected to
#define Josh2812_PIXEL_BIT   4      // Bit of the pin the pixels are connected to

#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

#define RES 6000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays
#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives
#define CYCLES_PER_SEC (F_CPU)
#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )


#define bufsz 3*MAX_PIXELS
char buf [bufsz];
int pos;


void setup (void)
{

  for (int i=0; i<sizeof(spare_pins); i++) {
    pinMode(spare_pins[i], INPUT_PULLUP);
  }
  Serial.begin (BAUDRATE);   // debugging
  Serial.println ("\n---- started ----");
  Serial.print ("FILE: ");
  Serial.println (__FILE__);
  Serial.print ("BUILT: ");
  Serial.print (__DATE__);
  Serial.print (" ");
  Serial.println (__TIME__);
#ifdef POLLED_MODE
  Serial.println ("Polled mode active");
#else
  Serial.println ("Int mode active");
#endif

  delay(100);
  setup_Adafruit();

  setup_SPI();

  Josh2812_ledsetup();
 
  colourfade_strip();

// WARNING!!! interrupts are DISABLED in polled mode
// this is needed to run at 2 Mbit/sec
#ifdef POLLED_MODE
  cli();  
#endif
}

unsigned int temp_loopctr = 0;

unsigned int loopctr = 0;
byte c;
byte t;
void loop (void) {
  while (!(SPSR & (1<<SPIF))) {
    if (loopctr++ > 250) {    // 250

#ifdef ENABLE_UART
#ifdef POLLED_MODE
    if (UCSR0A & (1<<RXC0)) handle_uart();
#else // interrupts are available
    if (Serial.available()>0) handle_uart();
#endif
#endif //ENABLE_UART
      loopctr = 0;
      if (pos >= 1) {  // have we had at least 1 byte?

        if (pos%3 == 0) { // check for divisibility by 3, if not we know its corrupt
#ifndef POLLED_MODE
          cli();    // disable interrupts while we blat out the string
#endif
          for (int i=0; i<pos; i++) {
            Josh2812_sendByte(buf[i]);
          }
#ifndef POLLED_MODE
          sei();  // and re-enable them
#endif
        }
        Josh2812_show();
        pos=0;
      }
    }
#ifdef ENABLE_UART
    if (update_ada) {
      update_ada = 0;
      strip.show();
#ifdef POLLED_MODE
      cli(); // strip.show always re-enabled interrupts
#endif
    }
#endif // ENABLE_UART
  }
  buf[pos++] = SPDR;
  if (pos>=bufsz) { pos=bufsz-1; }
  loopctr = 0;
}


void handle_uart() {
#ifdef POLLED_MODE
  c=UDR0;
  UDR0 = c;
#else // interrupts are available
  c=Serial.read();
  Serial.write(':');
  Serial.println(c,HEX);
#endif
  ada_buffer[ada_ptr++] = c;
  if (ada_ptr >= sizeof ada_buffer) {
    ada_ptr--;
  }
  if (c=='\n') {
    ada_ptr = 0;
      if (ada_buffer[8] == '\n') {
        ada_buffer[8] = 0;
        strip.setPixelColor(
          16*ascii2hex(ada_buffer[0])+ascii2hex(ada_buffer[1]),
          16*ascii2hex(ada_buffer[2])+ascii2hex(ada_buffer[3]),
          16*ascii2hex(ada_buffer[4])+ascii2hex(ada_buffer[5]),
          16*ascii2hex(ada_buffer[6])+ascii2hex(ada_buffer[7])
        );
        update_ada = 1;
      }
      if (ada_buffer[4] == '\n') {
        ada_buffer[4] = 0;
        strip.setPixelColor(
          16*ascii2hex(ada_buffer[0]),
          16*ascii2hex(ada_buffer[1]),
          16*ascii2hex(ada_buffer[2]),
          16*ascii2hex(ada_buffer[3])
        );
        update_ada = 1;
      }
  }
}


void Josh2812_sendBit( bool bitVal ) { 
    if (  bitVal ) {				// 0 bit      
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
			[port]		"I" (_SFR_IO_ADDR(Josh2812_PIXEL_PORT)),
			[bit]		"I" (Josh2812_PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T1H) - 2),	// 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
			[offCycles] 	"I" (NS_TO_CYCLES(T1L) - 2)	// Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness
		);
                                  
    } else {					// 1 bit

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
			[port]		"I" (_SFR_IO_ADDR(Josh2812_PIXEL_PORT)),
			[bit]		"I" (Josh2812_PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T0H) - 2),
			[offCycles]	"I" (NS_TO_CYCLES(T0L) - 2)

		);
      
    }   
    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
    // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
    // This has thenice side effect of avoid glitches on very long strings becuase     
}  

void Josh2812_sendByte( unsigned char byte ) {    
    for( unsigned char bit = 0 ; bit < 8 ; bit++ ) {
      Josh2812_sendBit( bitRead( byte , 7 ) );                // Neopixel wants bit in highest-to-lowest order
                                                     // so send highest bit (bit #7 in an 8-bit byte since they start at 0)
      byte <<= 1;                                    // and then shift left so bit 6 moves into 7, 5 moves into 6, etc
    }           
} 

/*

  The following three functions are the public API:
  
  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.  
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame. 
  
*/

void Josh2812_ledsetup() {  
  bitSet( Josh2812_PIXEL_DDR , Josh2812_PIXEL_BIT );
  cli();
  for (int i=0; i< 2000; i++)     Josh2812_sendPixel( 0,0,0 );
  sei();
}

void Josh2812_sendPixel( unsigned char r, unsigned char g , unsigned char b )  {  
  Josh2812_sendByte(g);          // Neopixel wants colors in green then red then blue order
  Josh2812_sendByte(r);
  Josh2812_sendByte(b);
}


// Just wait long enough without sending any bits to cause the pixels to latch and display the last sent frame
void Josh2812_show() {
	_delay_us( (RES / 1000UL) + 1);				// Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}

void setup_SPI() {
  SPI.setClockDivider(SPI_CLOCK_DIV8);    // irrelevant really, we go slave mode 
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  // turn on SPI in slave mode
  SPCR |= bit (SPE);
  // have to send on master in, *slave out*
  pinMode(MISO, OUTPUT);
}

void colourfade_strip() {
  #define SLOW 4
  #define FAST 32
  int r=0;  int g=0;  int b=0;
  int step = SLOW;
  int junk = SPSR;
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (r=0; r<=255; r+=step) fill( r,0,0,0 ); // black to red
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (g=0; g<=255; g+=step) fill( 255,g,0,0 ); // red to yellow
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (r=255; r>=0; r-=step) fill( r,255,0,0 ); // yellow to green
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (b=0; b<=255; b+=step) fill( 0,255,b,0 );  // green to cyan
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (g=255; g>=0; g-=step) fill( 0,g,255,0 );  // cyan to blue
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (r=0; r<=255; r+=step) fill( r,0,255,0 );  // blue to purple
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (g=0; g<=255; g+=step) fill( 255,g,255,0 );  // purple to white
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  for (g=255; g>=0; g-=(step/2)) fill( g,g,g,0 );  // white to black
  if ((SPSR & (1<<SPIF))) { step = FAST; }
  fill( 0,0,0,0 );  // force black
}

void setup_Adafruit() {
  strip.begin();            // Init LED strand, set all black, then all to startcolor
  strip.setBrightness( (255 / 100) * ADA_BRIGHTNESS );
}

void fill (byte _r, byte _g, byte _b, int _pause) {
  cli();
  for (int i=0; i< 200; i++)     Josh2812_sendPixel( _r,_g,_b );
  sei();
  for ( int i=0; i < ADA_LEDCOUNT; i++ ) {     // For each LED
    strip.setPixelColor( i, _r,_g,_b );      // .. set the color
  }
  strip.show();

  delay(_pause);
}  

byte ascii2hex(char asc) {
  if ((asc >= '0') && (asc <= '9')) {
    return asc-'0';
  }
  else if ((asc >= 'a') && (asc <= 'f')) {
    return 10+asc-'a';
  }      
  else if ((asc >= 'A') && (asc <= 'F')) {
    return 10+asc-'A';
  }    
  else {
    return 0;
  }  
}

