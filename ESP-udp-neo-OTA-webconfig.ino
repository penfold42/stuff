/*
Flashchip IDs:
ESP201      0x1340ef  #define WINBOND_NEX_W25Q40_V  0x4013  4Mbit = 512K BYTE
ESP01-black 0x1440c8  #define GIGADEVICE_GD25Q80  0x4014   8Mbit = 1M BYTE
ESP01-blue  0x1340c8  #define GIGADEVICE_GD25Q40  0x4013   4Mbit = 512K BYTE
            0x1640c8  #define GIGADEVICE_GD25Q32  0x4016  32Mbit = 4M BYTE
ebay chip   0x1640ef  #define WINBOND_NEX_W25Q32_V  0x4016  32Mbit = 4M BYTE

WiFi WS2812 LED strip controller 
- UDP binary packets
- UDP ascii packets
- UDP status info
- HTTP status info
- HTTP and OTA updates
- mDNS-SD service advertisement
- HTTP based WiFi configuration



ASCIIPort protocol:
Lrgb\n  
LLrrggbb\n
LLLrrggbb\n
Led#, red, green, blue as ascii hex
L can be '*' for all leds
\n can be \r or ^ to pack multiple updates in one packet
RESET
STATUS
VERSION
BLANK set all leds to 222
ERASE - clear the WiFi settings

RAW protocol.
no parsing, simply send RGBRGBRGBRGB to set a string of LEDs from #0

RAW Delta protocol
[deleted....]

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <NeoPixelBus.h>
#include <ArduinoOTA.h>

// httpupdate includes
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// wifimanager includes
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager

#define THIS_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)



ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiManager wifiManager;

unsigned int ASCIIPort = 2390;      // local port to listen for UDP packets
unsigned int RAWPort = 2801;      // local port to listen for UDP packets
unsigned int RAWDeltaPort = 2392;      // local port to listen for UDP packets

unsigned long ASCII_ctr = 0;
unsigned long RAW_ctr = 0;

// NeoPixel defines.
// NOTE!!! NeoPin is IGNORED by all but the NeoEsp8266BitBang800KbpsMethod method
#define NeoPin 3
#define PixelCount 350

// know methods: {UseBigBang, UseUart, UseAsyncUart, UseDma}
#define UseDma
//#define UseAsyncUart
//#define UseUart

#if defined UseUart
  NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount, 2); // GPIO2 always
  #define which_method "Esp8266Uart800Kbps"
  #if (NeoPin != 2)
    #error NeoPin must be 2 for this method
  #endif
#elif defined UseAsyncUart
  NeoPixelBus<NeoGrbFeature, NeoEsp8266AsyncUart800KbpsMethod> strip(PixelCount, 2); // GPIO2 always
  #define which_method "Esp8266AsyncUart800Kbps"
  #if (NeoPin != 2)
    #error NeoPin must be 2 for this method
  #endif
#elif defined UseDma
  NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, 3); // GPIO 3/RXD always
  #define which_method "Esp8266Dma800Kbps"
  #if (NeoPin != 3)
    #error NeoPin must be 3 for this method
  #endif
#elif defined UseBigBang
  NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(PixelCount, NeoPin);
  #define which_method "Esp8266BitBang800Kbps"
#else
  #error Neo Method is not known or defined. know methods: {UseBigBang, UseUart, UseAsyncUart, UseDma}
#endif




// struct and object to support direct element access
struct GrbPixel {
    uint8_t G;
    uint8_t R;
    uint8_t B;
};
GrbPixel* pixels = (GrbPixel*) strip.Pixels();

// how bright are the status LEDs on startup?
#define startup_brite 4

//sets brightness multiplier
#define web_brightness 8

// hostname used for config AP name and mDNS advertisements
String my_hostname;

// uptime based counters
long unsigned int temp_mill;
long unsigned int this_mill;
long unsigned int last_mill;
unsigned long long  extended_mill;
unsigned long long mill_rollover_count;

// buffer to hold snprintf created responses
#define TEMP_STR_LEN 80
char temp_str[TEMP_STR_LEN];

// buffer to hold incoming and outgoing UDP packets
#define PACKET_BUFFER_SIZE 3*PixelCount+4
char packetBuffer[ PACKET_BUFFER_SIZE]; 

// 
WiFiManagerParameter custom_ascii_port("asc_port", "ascii port", "2390", 6);
WiFiManagerParameter custom_raw_port("raw_port", "raw port", "2801", 6);

// .bmp generator
char name[] = "9px_0000.bmp";       // filename convention (will auto-increment)
//const int w = 16;                   // image width in pixels
//const int h = 9;                    // " height
#define w  16                   // image width in pixels
#define h   9                    // " height
const int imgSize = w*h;
int px[w*h];                        // actual pixel data (grayscale - added programatically below)



// A UDP instance to let us send and receive packets over UDP
WiFiUDP ASCIIudp;
WiFiUDP RAWudp;


void rainbow_flash(int brightness ) {
  int rainbows[6][3] = {
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 1, 1},
    {0, 0, 1},
    {1, 0, 1}
  };

  for (int i=0; i<6; i++) {
    fill_and_show ( brightness*(rainbows[i][0]), 
                    brightness*(rainbows[i][1]), 
                    brightness*(rainbows[i][2])
                    );
    delay(100);
    fill_and_show (0,0,0);
    delay(100);  
  }

}
void setup() {

  Serial.begin(115200);

  strip.Begin();

  rainbow_flash(startup_brite);
  
  fill_and_show (startup_brite,0,0); // red for on, not ready
  
  Serial.print("\n\nChip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  my_hostname += "WS2812-";
  my_hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(my_hostname);

  // Print hostname.
  Serial.println("Hostname: " + my_hostname);

  Serial.print("FlashchipID: ");
  Serial.println(ESP.getFlashChipId(), HEX);

//  wifiManager.resetSettings();
  
  setup_wifimanager();

  fill_and_show( 0,startup_brite,0); // green is good

  Serial.println ("Start OTA server.");
  ArduinoOTA.setHostname((const char *)my_hostname.c_str());
  ArduinoOTA.begin();

  setup_http_server();

}

void configModeCallback (WiFiManager *myWiFiManager) {
  fill_and_show (startup_brite,0,startup_brite); // purple - settings cleared, reseting

  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("saved config");
  for (int i=0; i<3; i++) {
    fill_and_show(startup_brite/2,0,startup_brite/2);  // dim purple
    delay(200);
    fill_and_show (startup_brite,0,startup_brite);   // full purple
    strip.Show();
    delay(200);
  }
}

void setup_wifimanager() {
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(180);

/*
  wifiManager.addParameter(&custom_ascii_port);
  wifiManager.addParameter(&custom_raw_port);
*/

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
//  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect(my_hostname.c_str())) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  fill_and_show( 0,startup_brite,0);
 
}

void setup_http_server(){
  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTP Server started. Open http://%s.local/update in your browser\n", my_hostname.c_str());

  httpServer.on("/bitmap", [](){
    send_bmp();
  });


  httpServer.on("/screen", [](){
    String message = "<html><head><title>WS2812 LED Receiver</title></head><body>";
    print_leds_table(message);    
    message += "</body></html>";
    message += "</body></html>";
    httpServer.send(200, "text/html", message);
  });

  
  httpServer.on("/status", [](){
    long int secs = (extended_mill/1000);
    long int mins = (secs/60);
    long int hours = (mins/60);
    long int days = (hours/24);
    
    String message = "<html><head><title>WS2812 LED Receiver</title></head><body>";

    message += "<TABLE>\r\n";

    message += "<TR><TH>Uptime<TD>"; message += days; message += " days, ";
    snprintf (temp_str, TEMP_STR_LEN,"%02d:%02d:%02d.%03d",hours%24, mins%60, secs%60, (int)extended_mill%1000);
    message += temp_str;
    message += "<TR><TH>FreeHeap<TD>"; message += ESP.getFreeHeap();
    message += "<TR><TH>Signal<TD>"; message += WiFi.RSSI();

    message += "<TR><TH>File<TD>"; message += THIS_FILE;
    message += "<TR><TH>Build Date<TD>" + String(__DATE__);
    message += " " + String(__TIME__);

    message += "<TR><TH>NeoMethod<TD>"; message += which_method;
    message += "<TR><TH>NeoPin<TD>"; message += NeoPin;

    message += "<TR><TH>raw packets<TD>"; message += RAW_ctr;
    message += "<TR><TH>ascii packets<TD>"; message += ASCII_ctr;

    message += "</TABLE>\r\n";
    
    message += "</body></html>";
    httpServer.send(200, "text/html", message);
  });
}

void print_leds_table(String & msg) {
  #define T_WIDTH 16
  #define T_HEIGHT 16

  msg += "<TABLE COLS=\"";
  msg += T_WIDTH;
  msg += "\">";
  for (int i=0; i< T_WIDTH*T_HEIGHT; i++) {
    if (i%T_WIDTH == 0) {
      msg += "\r\n<TR>";
    }       
    snprintf (temp_str, TEMP_STR_LEN, "<TD width=\"10\" height=\"10\" bgcolor=\"#%02x%02x%02x\">&nbsp", 
       std::min(web_brightness*pixels[i].R, 255),
       std::min(web_brightness*pixels[i].G, 255),
       std::min(web_brightness*pixels[i].B, 255)
       );
    msg += temp_str;
  }
  msg += "\r\n</TABLE>\r\n";
}


void loop()
{
  this_mill = millis();
  if (last_mill > this_mill) {  // rollover
    mill_rollover_count ++;
  }
  extended_mill = (mill_rollover_count << (8*sizeof(this_mill))) + this_mill;
  last_mill = this_mill;

  // Handle http update server
  httpServer.handleClient();

  // Handle OTA server.
  ArduinoOTA.handle();

  handle_raw_port();
  handle_ascii_port();

}


void handle_raw_port() {

  if (!RAWudp) {
    Serial.println("RE-Starting UDP");
    RAWudp.begin(RAWPort);
    MDNS.addService("hyperiond-rgbled", "udp", RAWPort);
    Serial.print("Local RAWport: ");
    Serial.println(RAWudp.localPort());
  }
  if (!RAWudp) {
    Serial.println("RE-Start failed.");
    return;
  }

  int cb = RAWudp.parsePacket();
  if (cb) {
    // We've received a packet, read the data from it
    RAWudp.read(packetBuffer, PACKET_BUFFER_SIZE); // read the packet into the buffer

    RAW_ctr++;

    int neo = 0;
    for (int i=0; (i<cb) && (i<PACKET_BUFFER_SIZE); i+=3) {
      if (neo < PixelCount) {
        pixels[neo].R = packetBuffer[i];
        pixels[neo].G = packetBuffer[i+1];
        pixels[neo].B = packetBuffer[i+2];
        neo++;
      }
    }
    strip.Dirty();
    strip.Show();
  } else {
    //    Serial.println("no packet yet");
  }
}



void   handle_ascii_port() {
  if (!ASCIIudp) {
    Serial.println("RE-Starting UDP");
    ASCIIudp.begin(ASCIIPort);
    Serial.print("Local ASCII port: ");
    Serial.println(ASCIIudp.localPort());
  }
  if (!ASCIIudp) {
    Serial.println("RE-Start failed.");
    return;
  }
  int cb = ASCIIudp.parsePacket();
  if (cb) {
    IPAddress remoteIP = ASCIIudp.remoteIP();
    int remotePort = ASCIIudp.remotePort();

    ASCII_ctr++;

    ASCIIudp.read(packetBuffer, PACKET_BUFFER_SIZE); // read the packet into the buffer

    if (cb < PACKET_BUFFER_SIZE) {
      packetBuffer[cb] = 0;
    }

    for (int i=0; (i<cb) && (i<PACKET_BUFFER_SIZE); i++) {
      if (packetBuffer[i] == '\r') {
        packetBuffer[i] = '\n';
      }
      if (packetBuffer[i] == '^') {
        packetBuffer[i] = '\n';
      }
    }

    byte neo, red, green, blue;
    if (strncmp(packetBuffer,"RESET\n", cb) == 0) {
      ESP.reset();
    }
    if (strncmp(packetBuffer,"STATUS\n", cb) == 0) {
      long int secs = (extended_mill/1000);
      long int mins = (secs/60);
      long int hours = (mins/60);
      long int days = (hours/24);
      
      memset(packetBuffer, 0, PACKET_BUFFER_SIZE);
      sprintf (packetBuffer, "Uptime: %d days, %02d:%02d:%02d.%03d\r\nFreeHeap: 0x%x\r\nFlashChipId: 0x%x\r\nSignal: %d\r\n", 
                  days, hours%24, mins%60, secs%60, (int) extended_mill%1000, ESP.getFreeHeap(), ESP.getFlashChipId(), WiFi.RSSI() );
      send_to_remote(remoteIP, remotePort, packetBuffer);           
    }
    if (strncmp(packetBuffer,"VERSION\n", cb) == 0) {
      memset(packetBuffer, 0, PACKET_BUFFER_SIZE);
      sprintf (packetBuffer, "File: %s\r\nBuild Date: %s %s\r\n",__FILE__,__DATE__, __TIME__); 
      send_to_remote(remoteIP, remotePort, packetBuffer);           
    
    }
    if (strncmp(packetBuffer,"BLANK\n", cb) == 0) {
      fill_and_show(0,0,0);
    }
    if (strncmp(packetBuffer,"ERASE\n", cb) == 0) {
      fill_and_show (startup_brite,0,startup_brite); // purple - settings cleared, reseting

      wifiManager.resetSettings();
      ESP.reset();
    }
    if (packetBuffer[4] == '\n') {
      for (int i=0; (i<(cb-4)) && (i<PACKET_BUFFER_SIZE); i+=5) {
        if (packetBuffer[i+4] == '\n') {
          packetBuffer[i+4] = 0;
          neo = ascii2hex(packetBuffer[i+0]);
          red = 16*ascii2hex(packetBuffer[i+1]);
          green = 16*ascii2hex(packetBuffer[i+2]);
          blue = 16*ascii2hex(packetBuffer[i+3]);
          RgbColor t = RgbColor( red, green, blue);
          if ((packetBuffer[i+0]) == '*') {
            fill_and_show (red, green, blue);            
          } else if (neo < PixelCount) {
            strip.SetPixelColor(neo, t);
          }
        }
      }
    }
    if (packetBuffer[8] == '\n') {
      for (int i=0; (i<(cb-8)) && (i<PACKET_BUFFER_SIZE); i+=9) {
        if (packetBuffer[i+8] == '\n') {
          packetBuffer[i+8] = 0;
          neo = 16*ascii2hex(packetBuffer[i+0])+ascii2hex(packetBuffer[i+1]);
          red = 16*ascii2hex(packetBuffer[i+2])+ascii2hex(packetBuffer[i+3]);
          green = 16*ascii2hex(packetBuffer[i+4])+ascii2hex(packetBuffer[i+5]);
          blue = 16*ascii2hex(packetBuffer[i+6])+ascii2hex(packetBuffer[i+7]);
          RgbColor t = RgbColor( red, green, blue);
          if ((packetBuffer[i+0]) == '*') {
            fill_and_show (red, green, blue);            
          } else if (neo < PixelCount) {
            strip.SetPixelColor(neo, t);
          }
        }
      }
    }
    if (packetBuffer[9] == '\n') {
      for (int i=0; (i<(cb-9)) && (i<PACKET_BUFFER_SIZE); i+=10) {
        if (packetBuffer[i+9] == '\n') {
          packetBuffer[i+9] = 0;
          neo = 256*ascii2hex(packetBuffer[i+0])+16*ascii2hex(packetBuffer[i+1])+ascii2hex(packetBuffer[i+2]);
          red = 16*ascii2hex(packetBuffer[i+3])+ascii2hex(packetBuffer[i+4]);
          green = 16*ascii2hex(packetBuffer[i+5])+ascii2hex(packetBuffer[i+6]);
          blue = 16*ascii2hex(packetBuffer[i+7])+ascii2hex(packetBuffer[i+8]);
          RgbColor t = RgbColor( red, green, blue);
          if ((packetBuffer[i+0]) == '*') {
            fill_and_show (red, green, blue);            
          } else if (neo < PixelCount) {
            strip.SetPixelColor(neo, t);
          }
        }
      }
    }
    strip.Show();
  } else {
//    Serial.println("no packet yet");
  }
}

void send_to_remote(IPAddress rIP, int rPort, char * pacBuf) {
    ASCIIudp.beginPacket(rIP, rPort); 
    ASCIIudp.write(pacBuf, strlen(pacBuf));
    ASCIIudp.endPacket();
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


int splitString(char separator, String input, String results[], int numStrings) {
    int idx;
    int last_idx=0;
    int retval = 0;
//    message += "numStrings: ";
//    message += numStrings;
//    message += "\n";

    for (int i=0; i<numStrings; i++) {
      results[i] = "";     // pre clear this 
      idx = input.indexOf(separator,last_idx);
// message += "i: " ; message += i; 
// message += " idx: " ; message += idx; 
// message += " last_idx: " ; message += last_idx; 
      if ((idx == -1)&&(last_idx == -1)) {
         break;
      } else {
        results[i] = input.substring(last_idx,idx);
        retval ++;
// message += " results: "; message += results[i];
// message += "\n";
        if (idx != -1) {
          last_idx = idx+1;
        } else {
          last_idx = -1;
        }
      }
    }
    return retval;
}

void fill_and_show (byte r, byte g, byte b) {
  for (int neo=0; neo < PixelCount; neo++) {
    pixels[neo].R = r;
    pixels[neo].G = g;
    pixels[neo].B = b;
    strip.Dirty();
  }
  strip.Show();
}

void fill_noshow (byte r, byte g, byte b) {
  for (int neo=0; neo < PixelCount; neo++) {
    pixels[neo].R = r;
    pixels[neo].G = g;
    pixels[neo].B = b;
    strip.Dirty();
  }
}

void send_bmp() {

  // iteratively create pixel data
  int increment = 256/(w*h);        // divide color range (0-255) by total # of px
  for (int i=0; i<imgSize; i++) {
    px[i] = i * increment;          // creates a gradient across pixels for testing
  }

  // set fileSize (used in bmp header)
  int rowSize = 4 * ((3*w + 3)/4);      // how many bytes in the row (used to create padding)
  int fileSize = 54 + h*rowSize;        // headers (54 bytes) + pixel data

  // create image data; heavily modified version via:
  // http://stackoverflow.com/a/2654860
  unsigned char *img = NULL;            // image data
  if (img) {                            // if there's already data in the array, clear it
    free(img);
  }
  img = (unsigned char *)malloc(3*imgSize);

  for (int y=0; y<h; y++) {
    for (int x=0; x<w; x++) {
      int colorVal = px[y*w + x];                        // classic formula for px listed in line
      img[(y*w + x)*3+0] = (unsigned char)(colorVal);    // R
      img[(y*w + x)*3+1] = (unsigned char)(colorVal);    // G
      img[(y*w + x)*3+2] = (unsigned char)(colorVal);    // B
      // padding (the 4th byte) will be added later as needed...
    }
  }


  // create padding (based on the number of pixels in a row
  unsigned char bmpPad[rowSize - 3*w];
  for (int i=0; i<sizeof(bmpPad); i++) {         // fill with 0s
    bmpPad[i] = 0;
  }

  // create file headers (also taken from StackOverflow example)
  String bmpFileHeader =             // file header (always starts with BM!)
    "BM\0\0\0\0\0\0\0\0\66\0\0\0";
  String bmpInfoHeader =             // info about the file (size, etc)
    "\50\0\0\0\0\0\0\0\0\0\0\0\1\0\30\0"   ;

  bmpFileHeader.setCharAt( 2, (unsigned char)(fileSize      ));
  bmpFileHeader.setCharAt( 3, (unsigned char)(fileSize >>  8));
  bmpFileHeader.setCharAt( 4, (unsigned char)(fileSize >> 16));
  bmpFileHeader.setCharAt( 5, (unsigned char)(fileSize >> 24));

  bmpInfoHeader.setCharAt( 4, (unsigned char)(       w      ));
  bmpInfoHeader.setCharAt( 5, (unsigned char)(       w >>  8));
  bmpInfoHeader.setCharAt( 6, (unsigned char)(       w >> 16));
  bmpInfoHeader.setCharAt( 7, (unsigned char)(       w >> 24));
  bmpInfoHeader.setCharAt( 8, (unsigned char)(       h      ));
  bmpInfoHeader.setCharAt( 9, (unsigned char)(       h >>  8));
  bmpInfoHeader.setCharAt(10, (unsigned char)(       h >> 16));
  bmpInfoHeader.setCharAt(11, (unsigned char)(       h >> 24));

  // write the file (thanks forum!)
  String bmpfile = bmpFileHeader;  // write file header
  bmpfile += bmpInfoHeader;         // " info header

  for (int i=0; i<h; i++) {                            // iterate image array
//    fwrite(img+(w*(h-i-1)*3), 3*w,1, stdout);                // write px data
//    fwrite(bmpPad, (4-(w*3)%4)%4,1, stdout);                 // and padding as needed
    for (int x=(w*(h-i-1)*3); x<(w*(h-i-1)*3)+3*w; x++) {
      bmpfile += img[x];
    }
    for (int x=0; x<(4-(w*3)%4)%4; x++) {
      bmpfile += bmpPad[x];                 // and ROW padding as needed
    }
  }

  httpServer.send(200, "image/bmp", bmpfile);

}
