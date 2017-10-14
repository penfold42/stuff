/*
Generic GPIO interface via the web interface
/uptime
/reset
/version
/acl/<row>/set/aaa.bbb.ccc.ddd.eee  // add aaa.bbb.ccc.ddd to the allowed IP list in slot <row>

/gpio/<n>/get      // read gpio <n>
/gpio/<n>/set/0    // set to low
/gpio/<n>/set/1    // set to high
/gpio/<n>/mode/0   // set to input
/gpio/<n>/mode/1   // set to output
/gpio/<n>/toggle/101  // toggle the output 1,0,1 with 200ms delay

/neo/<n>/<n>/r,g,b

CHANGELOG:
added extended_mill to catch and count rollover of millis() after 24/48 days
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <NeoPixelBus.h>


int acl_check();  // with 1.6.7 it couldnt find the forward reference...

#define NeoPin 2  // undefine this to remove all neo code
#define PixelCount 3
#define startup_brite 8

// defines for the sonar function
//#define sonar_trig_pin 0
//#define sonar_echo_pin 2
 
const char* ssid = "yourssid";
const char* password = "yourpassword";
//MDNSResponder mdns;
ESP8266WebServer server(80);

#ifdef NeoPin
  #define colorSaturation 128
//  NeoPixelBus strip = NeoPixelBus(PixelCount, NeoPin);
  NeoPixelBus<NeoGrbFeature, NeoEsp8266AsyncUart800KbpsMethod> strip(PixelCount, 2); // GPIO2 always
// struct and object to support direct element access
  struct GrbPixel {
      uint8_t G;
      uint8_t R;
      uint8_t B;
  };
  GrbPixel* pixels = (GrbPixel*) strip.Pixels();
#endif


char temp_str[80];

// preconfigured list of allowed clients
//


String message;
String substrings[10];

long unsigned int this_mill;
long unsigned int last_mill;
unsigned long long  extended_mill;
unsigned long long mill_rollover_count;


void handleRoot() {
  server.send(200, "text/plain", "OK\n");
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

/*
void splitURI() {
    int idx;
    int last_idx=0;
    int offsets[10];
    for (int i=0; i<10; i++) {
      offsets[i] = -1;
      substrings[i] = "";     // pre clear this 
      idx = server.uri().indexOf('/',last_idx);
//message += idx; 
      offsets[i] = idx;
      if (idx == -1) {
      } else {
//message += ",";
        last_idx = idx+1;
      }
    }

//message += "\n";
    for (int i=0; i<10; i++) {
      if ((offsets[i] >= 0) && (offsets[i+1] > offsets[i])) {
        substrings[i] = server.uri().substring(offsets[i]+1,offsets[i+1]);
//message += i;
//message += ":" + substrings[i] + ":\n";
      } else if (offsets[i] >= 0) {
        substrings[i] = server.uri().substring(offsets[i]+1);
//message += i;
//message += ":" + substrings[i] + ":\n";
      } else {
        break;
      }
    }
}
*/

void handle_acl() {
  int acl = substrings[1].toInt();
//message += "got acl! :";
//message += acl;

  if ((acl < 1) || (acl >= 10)) {
    acl = -1;
    message += "ERROR\n";
  } else {
//message += " is valid\n";
      if (substrings[2] == "set") {
        String myresults[4];
        int res = splitString('.', substrings[3], myresults, sizeof(myresults)/sizeof(myresults[0]));
        long int ipasint = 0;
        ipasint += myresults[0].toInt();
        ipasint += myresults[1].toInt()<<8;
        ipasint += myresults[2].toInt()<<16;
        ipasint += myresults[3].toInt()<<24;
        allowed_hosts[acl] = ipasint;

        message += "acl ";
        message += acl;
        message += " = ";
        message += substrings[3];
        message += " = ";
        message += ipasint;
        message += "\n";
      }
  }  
}

void handle_neo() {
  int neo = substrings[1].toInt();
//message += "got neo! :";
//message += neo;
  String tempstring = "";
  if ((neo < 0) || (neo >= PixelCount)) {
    message += "ERROR\n";
//message += " is INVALID\n";
  } else {
//message += " is valid\n";
      if (substrings[2] == "set") {
        String myresults[10];
        int res = splitString(',', substrings[3], myresults, sizeof(myresults)/sizeof(myresults[0]));
//      message += "input: " + substrings[3];
//      message += "\n res: ";
//      message += res;
        if (res == 3){
          sprintf (temp_str, "neo%d set to %d,%d,%d\r\n",neo, myresults[0].toInt(),  myresults[1].toInt(), myresults[2].toInt());
          message += temp_str;        
          RgbColor t = RgbColor( myresults[0].toInt(),  myresults[1].toInt(), myresults[2].toInt());
          strip.SetPixelColor(neo, t);
          strip.Show();
        } else {
              message += "ERROR\n";
        }
      }
  }  
}

void handle_gpio() {
  int gpio = substrings[1].toInt();
  if ((gpio == 0) && (substrings[1].charAt(0) != '0')) {
    gpio = -1;
  }
    
//message += "got gpio :";  message += gpio;
  switch (gpio) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
//message += " is valid\n";
      if (substrings[2] == "get") {
        sprintf (temp_str, "gpio%d is %d\r\n",gpio,digitalRead(gpio));
        message += temp_str;
      } else if (substrings[2] == "set") {
        int state = substrings[3].toInt();
        digitalWrite(gpio,state);
        sprintf (temp_str, "gpio%d set to %d\r\n",gpio,state);
        message += temp_str;        
      } else if (substrings[2] == "toggle") {
        for (int i=0; i<substrings[3].length(); i++) {
          int state = substrings[3].charAt(i)-0x30;
          digitalWrite(gpio,state);
          delay(200);
          sprintf (temp_str, "gpio%d set to %d\r\n",gpio,state);
          message += temp_str;
        }  
      } else if (substrings[2] == "mode") {
        if (substrings[3].charAt(0) == 'S') {
          pinMode(gpio, SPECIAL);
        }
        else if (substrings[3].charAt(0) == '?') {
// add query pin mode when i can work out how!
        } else {
          int state = substrings[3].toInt();
          pinMode(gpio,state);
          sprintf (temp_str, "gpio%d mode %d\r\n",gpio,state);
          message += temp_str;
        }        
      } else {
        message += "ERROR!\n";
      }      
      break;
    default:
      message += "ERROR!\n";
//message += " is INVALID\n";
  }
}

void handleNotFound(){
  message = "";

//  if (acl_check()) {
  if (1) {
//    splitURI();
    int res = splitString('/', server.uri().substring(1), substrings, sizeof(substrings)/sizeof(substrings[0]));
      
    if (substrings[0] == "gpio") {
      handle_gpio();
    } else if (substrings[0] == "acl") {
      handle_acl();
    } else if (substrings[0] == "reset") {
      message += "Rebooting in 1 sec....\n";
      server.send(200, "text/plain", message);
      delay(1000);
      ESP.reset();
    } else if (substrings[0] == "version") {
      message += "Build Date: ";
      message += __DATE__;
      message += " ";
      message += __TIME__;
      message += "\n";
      server.send(200, "text/plain", message);
#ifdef sonar_trig_pin
    } else if (substrings[0] == "sonar") {
        message += "Sonar: ";
        pinMode(sonar_trig_pin, OUTPUT);
        digitalWrite(sonar_trig_pin, LOW);
        delayMicroseconds(2);
        digitalWrite(sonar_trig_pin, HIGH);
        delayMicroseconds(10);
        digitalWrite(sonar_trig_pin, LOW);
        
        pinMode(sonar_echo_pin, INPUT);
        long int uS = pulseIn(sonar_echo_pin, HIGH);
        message += uS;
        message += " uS\n";
        message += uS/58;
        message += " cm\n";
#endif

#ifdef NeoPin
    } else if (substrings[0] == "neo") {
      handle_neo();
#endif
    
    } else {
      message += "ERROR!\n";
//     dumpVars();
    }
    server.send(200, "text/plain", message);
  } else {
    server.send(403, "text/plain", "Unauthorized\n");
  }

}

void dumpVars(){
    message += "File Not Found\n\n";
    message += "\nURI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i=0; i<server.args(); i++){
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
}

int acl_check() {  
  int ret_code = 0;
  unsigned int temp = server.client().remoteIP();
//    message += "\nClient: ";
//    message += server.client().remoteIP()[0];    message += ".";
//    message += server.client().remoteIP()[1];    message += ".";
//    message += server.client().remoteIP()[2];    message += ".";
//    message += server.client().remoteIP()[3];    message += "\n";
    
    for (int i=0; i<10; i++) {
//message += i;
      if (allowed_hosts[i] == temp) {
//        message += " match!\n";
        ret_code = 1;
        break; // found it - bail the loop
      } else {
//        message += " not match\n";
        ret_code = 0;
      }     
    }   
  return ret_code;
}

void old_fill_and_show (byte r, byte g, byte b) {
#ifdef NeoPin
  for (int neo=0; neo < PixelCount; neo++) {
    pixels[neo].R = r;
    pixels[neo].G = g;
    pixels[neo].B = b;
    strip.Dirty();
  }
  strip.Show();
#endif
}

void fill_and_show (byte r, byte g, byte b) {
#ifdef NeoPin
  RgbColor t = RgbColor( r,g,b );
  strip.ClearTo(t);
  strip.Show();
#endif
}

void setup(void){
  pinMode(2, OUTPUT);
  digitalWrite(2, 1);
  pinMode(0, OUTPUT);
  digitalWrite(0, 1);
  
  Serial.begin(115200);

  #ifdef NeoPin
    strip.Begin();
    strip.Show();
    fill_and_show (startup_brite,0,0); // red for on, not ready
  #endif
  
  Serial.print("\n\nChip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);
  Serial.print("FlashchipID: ");
  Serial.println(ESP.getFlashChipId(), HEX);

  Serial.println("\n\n\nWaiting for autoconnect");
  delay(1000);
    
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected, setting station mode");
    WiFi.mode(WIFI_STA);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  }
  Serial.println("");

  // Wait for connection
  int connect_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (connect_count&1){
      fill_and_show( startup_brite,startup_brite,0); // yellow
    } else {
      fill_and_show( startup_brite/2,startup_brite/2,0); // yellow      
    }
    delay(500);
    Serial.print(".");
    if (connect_count++ > 20) {
      fill_and_show( startup_brite,0,startup_brite); // purple
      Serial.println("\nGiving up, resetting.\n\n");
      delay(500);
      ESP.reset();      
    }
  }
  fill_and_show( 0,startup_brite,0); // green is good
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

/*
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
*/

  server.on("/", handleRoot);
  
  server.on("/uptime", [](){
//    long unsigned int mill = millis();
//    long int secs = (mill/1000);
    long int secs = (extended_mill/1000);
    long int mins = (secs/60);
    long int hours = (mins/60);
    long int days = (hours/24);
    
    sprintf (temp_str, "Uptime: %d days, %02d:%02d:%02d.%03d\r\nFreeHeap: %x\r\nSignal: %d\r\n", 
                days, hours%24, mins%60, secs%60, (int)extended_mill%1000, ESP.getFreeHeap(), WiFi.RSSI() );
    String message = temp_str;
    server.send(200, "text/plain", message);
  });

  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started");


}


void loop(void){
//  mdns.update();
  server.handleClient();
  if (Serial.available()) {
    Serial.read();
  }
  this_mill = millis();
  if (last_mill > this_mill) {  // rollover
    mill_rollover_count ++;
  }
  extended_mill = (mill_rollover_count << (8*sizeof(this_mill))) + this_mill;
  last_mill = this_mill;
} 
