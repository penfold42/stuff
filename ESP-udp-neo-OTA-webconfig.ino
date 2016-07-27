/*
Flashchip IDs:
ESP201      0x1340ef  #define WINBOND_NEX_W25Q40_V  0x4013  4Mbit = 512K BYTE
ESP01-black 0x1440c8  #define GIGADEVICE_GD25Q80  0x4014   8Mbit = 1M BYTE
ESP01-blue  0x1340c8  #define GIGADEVICE_GD25Q40  0x4013   4Mbit = 512K BYTE
            0x1640c8  #define GIGADEVICE_GD25Q32  0x4016  32Mbit = 4M BYTE
ebay chip   0x1640ef  #define WINBOND_NEX_W25Q32_V  0x4016  32Mbit = 4M BYTE

UDP packet to WS2812 bridge.


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

RAW protocol.
no parsing, simply send RGBRGBRGBRGB to set a string of LEDs from #0

RAW Delta protocol

Byte 0 
[7..5] frame type
  0 normal update
  1 set soft max leds
  2 set auto update rate
  
[0.4] update sequence number
  
byte 1
  [7] this is max frag
  [012] fragment number

Byte 2
  [0..6] start led index high boys
Byte 3
  [0..7] start led index low byte

Byte 4.. End
3 bytes per led in sequence RGB

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

//#include <wifi_defaults.h>  // my personal wifi settings in arduino\libraries\my_defaults\wifi_defaults.h

#define HOSTNAME "ESP8266-" ///< Hostename. The setup function adds the Chip ID at the end.

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiManager wifiManager;

#define NeoPin 2
#define PixelCount 750
//NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(PixelCount, NeoPin);
//NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount, NeoPin);

NeoPixelBus<NeoGrbFeature, NeoEsp8266AsyncUart800KbpsMethod> strip(PixelCount, NeoPin); // GPIO2

//NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, NeoPin); // GPIO 3/RXD !!

// struct and object to support direct element access
struct GrbPixel {
    uint8_t G;
    uint8_t R;
    uint8_t B;
};
GrbPixel* pixels = (GrbPixel*) strip.Pixels();

// how bright are the status LEDs on startup?
#define startup_brite 4

String my_hostname;

long unsigned int temp_mill;

long unsigned int this_mill;
long unsigned int last_mill;
unsigned long long  extended_mill;
unsigned long long mill_rollover_count;


unsigned int ASCIIPort = 2390;      // local port to listen for UDP packets
unsigned int RAWPort = 2801;      // local port to listen for UDP packets
unsigned int RAWDeltaPort = 2392;      // local port to listen for UDP packets

#define PACKET_BUFFER_SIZE 3*PixelCount+4
char packetBuffer[ PACKET_BUFFER_SIZE]; //buffer to hold incoming and outgoing packets

#define NUM_BUFFERS 4
#define NUM_SEQ 8
char buffers[NUM_BUFFERS][3*PixelCount];
int8_t sequence_frag_state[NUM_SEQ];  // which fragments are valid for this sequence?
int8_t sequence_to_buffer[NUM_SEQ] = {-1, -1, -1, -1, -1, -1, -1, -1}; // in which of the 4 buffers is this sequence# stored in?
int8_t buffer_to_sequence[NUM_BUFFERS] = {-1, -1, -1, -1}; // which sequence does this buffer hold?



// A UDP instance to let us send and receive packets over UDP
WiFiUDP ASCIIudp;
WiFiUDP RAWudp;
WiFiUDP RAWDeltaudp;

void setup() {

  Serial.begin(115200);

  strip.Begin();
  
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  my_hostname += HOSTNAME;
  my_hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(my_hostname);

  // Print hostname.
  Serial.println("Hostname: " + my_hostname);

  fill (startup_brite,0,0); // red for on, not ready
  strip.Show();

  Serial.print("FlashchipID: ");
  Serial.println(ESP.getFlashChipId(), HEX);

  wifiManager.resetSettings();
  
  setup_wifimanager();
//  wifi_connect(); // old setup routine pre wifimanager

  fill( 0,startup_brite,0); // green is good
  strip.Show();

  Serial.println ("Start OTA server.");
  ArduinoOTA.setHostname((const char *)my_hostname.c_str());
  ArduinoOTA.begin();

  setup_httpupdate();
}

void configModeCallback (WiFiManager *myWiFiManager) {
  fill (startup_brite,0,startup_brite); // purple - settings cleared, reseting
  strip.Show();
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("saved config");
  for (int i=0; i<3; i++) {
    fill(startup_brite/2,0,startup_brite/2);  // dim purple
    strip.Show();
    delay(200);
    fill (startup_brite,0,startup_brite);   // full purple
    strip.Show();
    delay(200);
  }
}

void setup_wifimanager() {
  
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
//  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
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
  fill( 0,startup_brite,0);
  strip.Show();
 
}

void setup_httpupdate(){
  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer started. Open http://%s.local/update in your browser\n", my_hostname.c_str());
}

/*
char ssid[] = DEFAULT_SSID;  //  your network SSID (name)
char pass[] = DEFAULT_PASS;       // your network password
void wifi_connect() {
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    fill (startup_brite,startup_brite,0);   // yellow for connection
    strip.Show();
    delay(250);
    fill(startup_brite/2,startup_brite/2,0);  // flash yellow/dim-yellow
    strip.Show();
    delay(250);
    Serial.print(".");    
  }
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

}
*/

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
  handle_raw_delta_port();
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

    int neo = 0;
    for (int i=0; (i<cb) && (i<PACKET_BUFFER_SIZE); i+=3) {
      RgbColor t = RgbColor( packetBuffer[i], packetBuffer[i+1], packetBuffer[i+2]);
      if (neo < PixelCount) {
        strip.SetPixelColor(neo++, t);
      }
    }
    strip.Show();
  } else {
    //    Serial.println("no packet yet");
  }
}

void handle_raw_delta_port() {
  if (!RAWDeltaudp) {
    Serial.println("RE-Starting UDP");
    RAWDeltaudp.begin(RAWDeltaPort);
    Serial.print("Local RAWDeltaport: ");
    Serial.println(RAWDeltaudp.localPort());
  }
  if (!RAWDeltaudp) {
    Serial.println("RE-Start failed.");
    return;
  }

  int cb = RAWDeltaudp.parsePacket();
  if (!cb) {
  }
  else {
//    Serial.print("packet received, length=");
//    Serial.println(cb);
    // We've received a packet, read the data from it
    RAWDeltaudp.read(packetBuffer, PACKET_BUFFER_SIZE); // read the packet into the buffer
//      Serial.println (cb);
//      Serial.print (": ");


    if ( (cb>=4) && ((packetBuffer[0] & 0xf0)==0) ) {

      int dataLen = cb-4;
      int seqNum = packetBuffer[0] & 0x0f;
      int fragNum = packetBuffer[0] & 0x0f;
      int fragStart = 256*packetBuffer[2]+packetBuffer[3];
      

      // find the buffer for this sequence
      int bufNum = find_buffer_for_seq(seqNum);
      if (bufNum >=0) {
        for (int i=0; i<dataLen; i++) {
          if (fragStart+i < sizeof(buffers[0])) {
  //          buffers[bufNum][fragStart+i] = packetBuffer[4+i];
          }
        }
      }
    
      
// original code with no reassembly
      int neo = 256*packetBuffer[2]+packetBuffer[3];
      for (int i=4; (i<cb) && (i<PACKET_BUFFER_SIZE); i+=3) {
        RgbColor t = RgbColor( packetBuffer[i], packetBuffer[i+1], packetBuffer[i+2]);
        if (neo < PixelCount) {
          strip.SetPixelColor(neo++, t);
        }
      }
  
      
      temp_mill = millis();
      strip.Show();
      temp_mill = millis()-temp_mill;
  
//  #define DROP_PKTS
  #ifdef DROP_PKTS
      int drop_count = 0;
      while (RAWDeltaudp.parsePacket()) {
        drop_count++;
      }
      if (drop_count) {
         Serial.print ("drop_count is" );
         Serial.println (drop_count);
      }
  #endif
    }
  }
}

int find_buffer_for_seq(int seqNum) {
  
  int ret = sequence_to_buffer[seqNum];
  if (ret >= 0) {
    return ret;
  } else {
// need to find a free slot
    return -1; // give up, cant find one
    
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
  if (!cb) {
//    Serial.println("no packet yet");
  }
  else {
    IPAddress remoteIP = ASCIIudp.remoteIP();
    int remotePort = ASCIIudp.remotePort();
/*
    Serial.print("packet received, length=");
    Serial.print(cb);
    Serial.print(" from port");
    Serial.print(remotePort);
    Serial.print(" from IP");
    Serial.println(remoteIP);
*/
    ASCIIudp.read(packetBuffer, PACKET_BUFFER_SIZE); // read the packet into the buffer

    if (cb < PACKET_BUFFER_SIZE) {
      packetBuffer[cb] = 0;
    }

//      Serial.println (cb);
//      Serial.print (": ");
/*
    for (int i=0; (i<cb) && (i<PACKET_BUFFER_SIZE); i++) {
      Serial.write(packetBuffer[i]);
    }
    Serial.println("");
*/
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
      fill(0,0,0);
      strip.Show();
    }
    if (strncmp(packetBuffer,"ERASE\n", cb) == 0) {
      fill (startup_brite,0,startup_brite); // purple - settings cleared, reseting
      strip.Show();
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
//          if ((packetBuffer[i+0] & 0b11011111) == 'F') {
          if ((packetBuffer[i+0]) == '*') {
            fill (red, green, blue);            
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
//          if ((packetBuffer[i+0] & 0b11011111) == 'F') {
          if ((packetBuffer[i+0]) == '*') {
            fill (red, green, blue);            
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
            fill (red, green, blue);            
          } else if (neo < PixelCount) {
            strip.SetPixelColor(neo, t);
          }
        }
      }
    }
    strip.Show();
  }
//  delay(100);
}

void send_to_remote(IPAddress rIP, int rPort, char * pacBuf) {
    ASCIIudp.beginPacket(rIP, rPort); 
//    ASCIIudp.write(packetBuffer, PACKET_BUFFER_SIZE);
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

void fill (byte r, byte g, byte b) {
  for (int neo=0; neo < PixelCount; neo++) {
    strip.SetPixelColor(neo, RgbColor( r,g,b));
    pixels[neo].R = r;
    pixels[neo].G = g;
    pixels[neo].B = b;
    strip.Dirty();
  }
  strip.Show();
}

void fill_noshow (byte r, byte g, byte b) {
  for (int neo=0; neo < PixelCount; neo++) {
    strip.SetPixelColor(neo, RgbColor( r,g,b));
    pixels[neo].R = r;
    pixels[neo].G = g;
    pixels[neo].B = b;
    strip.Dirty();
  }
}

