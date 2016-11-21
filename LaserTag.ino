/* LaserTag
 *  ws2812 on pin 11
 *  Laser on pin 3
 *  IR sensor on pin 2
 *  trigger switch on pin 4 (5 ground)

https://github.com/cyborg5/IRLib2/

 */
//Decoded Sony(2): Value:52E9 Adrs:0 (15 bits) RED
//Decoded Sony(2): Value:32E9 Adrs:0 (15 bits) GREEN
//Decoded Sony(2): Value:72E9 Adrs:0 (15 bits) YELLOW
//Decoded Sony(2): Value:12E9 Adrs:0 (15 bits) BLUE


#include <NeoPixelBus.h>
const uint16_t PixelCount = 1; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 11;  // make sure to set this to the correct pin, ignored for Esp8266

NeoPixelBus<NeoGrbFeature,Neo800KbpsMethod> strip(PixelCount, PixelPin);
 
#include <IRLibDecodeBase.h> // First include the decode base
#include <IRLibSendBase.h>    // First include the send base

#include <IRLib_P02_Sony.h>  // to actually use. The lowest numbered
#include <IRLib_P11_RCMM.h>
#include <IRLibCombo.h>     // After all protocols, include this
// All of the above automatically creates a universal decoder
// class called "IRdecode" containing only the protocols you want.
// Now declare an instance of that decoder.
IRdecode myDecoder;
IRsend mySender;
// Include a receiver either this or IRLibRecvPCI or IRLibRecvLoop
#include <IRLibRecvPCI.h> 
IRrecvPCI myReceiver(2);  //pin number for the receiver

#define carrier 36  // 36 kHz IR Carrier frequency

// OLED display https://github.com/greiman/SSD1306Ascii
#include <SSD1306Ascii.h>
//#include <SSD1306AsciiAvrI2c.h>
#include <SSD1306AsciiWire.h>

// 0X3C+SA0 - 0x3C or 0x3D
#define OLED_I2C_ADDRESS 0x3C

//SSD1306AsciiAvrI2c oled;
SSD1306AsciiWire oled;

byte got_oled = false;  // was the oled display detected?

int hit_count = 0;      // how many times have i been hit
int fire_count = 0;     // how many times did it hit fire

float brite = 0.05f;

HslColor red(0.0, 1, brite);
HslColor yellow(0.16, 1, brite);
HslColor green(0.33, 1, brite );
HslColor blue(0.66,  1, brite);
HslColor white(0, 0, brite);
HslColor white3(0, 0, 3*brite);
HslColor black(0);
HslColor currentColor = HslColor();
float temp_hue;

void set_colour(HslColor blah) {
  currentColor = blah;
  strip.SetPixelColor(0, currentColor);
  strip.Show();  
}

char last_button;
char this_button;

//char ser;
uint8_t proto;
int32_t code;
int len;

int down_count;
signed long last_button_millis;
signed long last_led_millis;
signed long last_lcd_millis;

void setup() {
  pinMode(5,OUTPUT);    // switch ground
  digitalWrite(5,LOW);
  pinMode(4,INPUT);     // switch input
  digitalWrite(4,HIGH); // with pullup

  Serial.begin(115200);
//  delay(2000); while (!Serial); //delay for Leonardo

  strip.Begin();

  set_colour(red);

  setup_display();
  
  set_colour(green); 
  last_led_millis = millis()+1000;
  myReceiver.enableIRIn(); // Start the receiver
  Serial.println(F("Ready to receive IR signals"));
}

void setup_display() {
  Serial.print("setup_display\n");
  // Use A3/gpio17 as power
  pinMode(17,OUTPUT);
  digitalWrite(17,HIGH);

  Wire.beginTransmission(OLED_I2C_ADDRESS);
  byte error = Wire.endTransmission();
  if (error == 0) {
    got_oled = true;
    Serial.print(F("Found display at address: ")); Serial.println(OLED_I2C_ADDRESS,HEX);
    
    oled.begin(&Adafruit128x64, OLED_I2C_ADDRESS);
  //  oled.setFont(Adafruit5x7);  
    oled.setFont(Cooper26);
  
    oled.clear();  
    oled.println("Bang Bang!");
    oled.println("LaserTag");
  } else {
    got_oled = false;
    Serial.print(F("NO display at address: ")); Serial.println(OLED_I2C_ADDRESS,HEX);
  }
}

void update_display() {
  if (got_oled) {
    oled.clear();  
    oled.print("Shots "); oled.println(fire_count);
    oled.print("Hits "); oled.println(hit_count);
    last_lcd_millis = millis();   // reset led update counter
  }
}

void blank_display() {
  if (got_oled) {
    if ( signed(millis() - last_lcd_millis) > 2000 ) {
      oled.clear();
    }
  }
}

void do_led() {
  // set the led to a rainbow colour based on score
  if ( signed(millis() - last_led_millis) > 100 ) {
//Serial.print("  diff: ");Serial.print(signed(millis() - last_led_millis));
//Serial.print("  millis: ");Serial.print(millis());
//Serial.print("  last_led_millis: ");Serial.println(last_led_millis);
    last_led_millis = millis();
    temp_hue = float(hit_count%60)/60;
    set_colour(HslColor(temp_hue, 1, brite));
  }
}

void send_fire() {
  down_count = 0;
  mySender.send(SONY, 0x290,12, carrier);    // mute
  
  myReceiver.enableIRIn();      //Restart receiver       
  fire_count++;
  update_display();
}

void do_button() {
  if ( signed(millis() - last_button_millis) > 10 ) {
    last_button_millis = millis();
    this_button = digitalRead(4);
    
    if (this_button == 0) {    // button is pushed
      if (last_button == 1) {   // button has just been pushed
        send_fire();
      } else {  // button was held down
        if (down_count++ > 10) {
          send_fire();
        }
      }
    }
    last_button = this_button;
  }
}


void do_serial() {
  if (Serial.available()) {
    char ser = Serial.read();
    code = -1;
    
    switch (ser) {
      case 'r':
        proto = SONY; code = 0x52E9;  len=15;
        break;
      case 'g':
        proto = SONY; code = 0x32E9;  len=15;
        break;
      case 'b':
        proto = SONY; code = 0x72E9;  len=15;
        break;
      case 'y':
        proto = SONY; code = 0x12E9;  len=15;
        break;
      case 'm':
        proto = SONY; code = 0x290;  len=12; // mute
        break;
      default:
        break;
    }
    if (code >= 0) {
      mySender.send(proto, code, len, carrier);
      myReceiver.enableIRIn();      //Restart receiver
      Serial.print("key=");      Serial.print(ser);
      Serial.print(" code=");    Serial.println(code,HEX);
    }
  }  
}

void do_receive() {
  //Continue looping until you get a complete signal received
  if (myReceiver.getResults()) { 
    myDecoder.decode();           //Decode it
    if (myDecoder.protocolNum == SONY) {
      switch (myDecoder.value) {
        case 0x52E9:
          Serial.println("got red.");
          set_colour(red);
        break;
        case 0x32E9:
          Serial.println("got green.");
          set_colour(green);
        break;
        case 0x72E9:
          Serial.println("got yellow.");
          set_colour(yellow);
        break;
        case 0x12E9:
          Serial.println("got blue.");
          set_colour(blue);
        break;
        case 0x290:
          Serial.println(F("got mute."));
          hit_count+=1;
          last_led_millis = millis();   // reset led update counter
          set_colour(white3);
          update_display();
//          Serial.print(F("hit_count: "));  Serial.println(hit_count);
//          Serial.print(F("hue: "));  Serial.println(temp_hue);
        break;
        default:
          myDecoder.dumpResults(false);  //Now print results. Use false for less detail
      }
    }
    myReceiver.enableIRIn();      //Restart receiver
  }
}
void loop() {
  do_button();
  do_receive();
  do_serial();
  do_led();
  blank_display();
}

