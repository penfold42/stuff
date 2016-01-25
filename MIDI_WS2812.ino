#include <MIDI.h>

#include <Adafruit_NeoPixel.h>

#define brightness 8 // velocity/127/brightness
#define PIN 2  // undefine this to remove all neo code
#define pixelCount 60
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(pixelCount, PIN, NEO_GRB + NEO_KHZ800);

MIDI_CREATE_DEFAULT_INSTANCE();
// middle C is midi note 60 so maybe firstnote should be 30
#define firstnote 0x23
#define lastnote  (firstnote + pixelCount)

//int channel_pitch[18];
//int channel_velocity[18];
#define MAX_CHAN 18
#define MAX_NOTES 128
int channelpitch_velocity[MAX_CHAN][MAX_NOTES];

int dirty=1;
// -----------------------------------------------------------------------------

// This function will be automatically called when a NoteOn is received.
// It must be a void-returning function with the correct parameters,
// see documentation here:
// http://arduinomidilib.fortyseveneffects.com/a00022.html



void handleNoteOn(byte channel, byte pitch, byte velocity)
{
    // Do whatever you want when a note is pressed.
    dirty=1;
    channelpitch_velocity[channel][pitch] = velocity;
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
    // Do something when the note is released.
    // Note that NoteOn messages with 0 velocity are interpreted as NoteOffs.
    channelpitch_velocity[channel][pitch] = 0;
    dirty=1;
}

void handleCC(byte channel, byte number, byte value)
//https://www.nyu.edu/classes/bello/FMT_files/9_MIDI_code.pdf
//http://en.wikiaudio.org/MIDI:Channel_messages_tutorial#Mode_messages
{
  int numbervalue = number<<8 + value;
//  if ( (channel&0xF0) == 0xB0) {
    switch (numbervalue) {
      case 0x7800:
      case 0x797F:
      case 0x7B00:
        for (int i=0; i<MAX_CHAN; i++) {
          for (int j=0; j<MAX_NOTES; j++) {
            channelpitch_velocity[i][j]=0;          
          }
        }
    }
//  }
}
// -----------------------------------------------------------------------------

void setup()
{
    pixels.begin();

    pinMode(17,OUTPUT);

    do_swirl(pixels.Color(15,0,0));
    do_swirl(pixels.Color(0,15,0));
    do_swirl(pixels.Color(0,0,15));
 
    // Connect the handleNoteOn function to the library,
    // so it is called upon reception of a NoteOn.
    MIDI.setHandleNoteOn(handleNoteOn);  // Put only the name of the function

    // Do the same for NoteOffs
    MIDI.setHandleNoteOff(handleNoteOff);

    MIDI.setHandleControlChange(handleCC);
    
    // Initiate MIDI communications, listen to all channels
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();
}

void do_swirl (int this_colour) {
    for (int i=0; i<pixelCount; i++){
//      digitalWrite(17,HIGH);
      pixels.setPixelColor(i, this_colour); // Moderately bright green color.
      pixels.show();
      delay(3);

//      digitalWrite(17,LOW);
      pixels.setPixelColor(i, pixels.Color(0,0,0)); // Moderately bright green color.
      pixels.show();
      delay(3);
    }
}    

void loop()
{

    // Call MIDI.read the fastest you can for real-time performance.
    MIDI.read();

    if (dirty == 1){
      compute_leds();
      pixels.show();
      dirty=0;
    }
    yield();
}

void OLD_loop()
{

    for (int i=0; i<100; i++) {
    // Call MIDI.read the fastest you can for real-time performance.
      MIDI.read();

      if ((i == 0) && (dirty == 1)){
        compute_leds();
        pixels.show();
        dirty=0;
      }
    }
}

void compute_leds() {
  float r,g,b;
  int channel, pitch, velocity;
  pixels.clear();
  for (int channel=16; channel>=1; channel--) {
    for (int pitch = firstnote; pitch<lastnote; pitch++) {
      velocity = channelpitch_velocity[channel][pitch];
      if ((pitch>=firstnote) && (pitch<lastnote) && (velocity>0)) {
        HSVtoRGB( &r,  &g,  &b, 360*(channel-1)/15, 1, (float) velocity/127/brightness );
        pixels.setPixelColor(pitch-firstnote, pixels.Color(r*255, g*255, b*255)); // Moderately bright green color.
      }
    }
  }
  pixels.show();
}

//The hue value H runs from 0 to 360º. 
//The saturation S is the degree of strength or purity and is from 0 to 1. 
//  Purity is how much white is added to the color, so S=1 makes the purest color (no white). 
//Brightness V also ranges from 0 to 1, where 0 is the black.

void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
  int i;
  float f, p, q, t;

  if( s == 0 ) {
    // achromatic (grey)
    *r = *g = *b = v;
    return;
  }

  h /= 60;      // sector 0 to 5
  i = floor( h );
  f = h - i;      // factorial part of h
  p = v * ( 1 - s );
  q = v * ( 1 - s * f );
  t = v * ( 1 - s * ( 1 - f ) );

  switch( i ) {
    case 0:
      *r = v;
      *g = t;
      *b = p;
      break;
    case 1:
      *r = q;
      *g = v;
      *b = p;
      break;
    case 2:
      *r = p;
      *g = v;
      *b = t;
      break;
    case 3:
      *r = p;
      *g = q;
      *b = v;
      break;
    case 4:
      *r = t;
      *g = p;
      *b = v;
      break;
    default:    // case 5:
      *r = v;
      *g = p;
      *b = q;
      break;
  }

}

