// Configure these things

// WiFi, cuz we need that.
const char* wifissid = "ssid";
const char* wifipsk  = "password";

// Debug: 0 = off. 1 = print debug info to serial port
// debug should only be used when you can act as the motorcycle
#define DEBUG 0

// How frequently to poll the OBD-II port for data? 60k = 1m
#define POLLING_INTERVAL 60000

// Enable the garage door open function for a button on GPIO0
// https://github.com/patfreeman/garage_door_closer
// if enabling this, go down to the bottom of this file and update the settings
#define GARAGE_DOOR_OPENER 0

// End configs

// Debugging print functions
#define dprint(a)   do { if (DEBUG) { Serial.print(a);   } } while(0)
#define dprintln(a) do { if (DEBUG) { Serial.println(a); } } while(0)

// Various variables to store status info
unsigned long last_poll = 0;   // time of last time we started polling info
String input;
int receiving = 0;             // are we actively getting data from the serial port?
unsigned long last_toggle = 0; // time of the last toggle
int toggling = 0;              // are we actively toggling
String sent_command;           // active command being sent

// Data we track
int soc = 0;     // State of Charge
int fg = 0;      // Fuel Gauge
float psv = 0;   // Pack Sum Voltage
float sapsv = 0; // Sag-Adj Pack Sum Voltage
int lcv = 0;     // Least Cell Voltage
int hcv = 0;     // Highest Cell Voltage
int mt = 0;      // Motor Temperature
int ct = 0;      // Controller Temperature
int bt = 0;      // Board Temperature
int at = 0;      // Ambient Temperature

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

void setup() {
  Serial.begin(38400);
  dprintln("Connecting to " + String(wifissid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifissid, wifipsk);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    dprint(".");
  }
  dprint("WiFi connected with IP address: ");
  dprintln(WiFi.localIP());

  server.on("/json",                  [](){ server.send(200, "text/plain", String(make_json())); });
  server.on("/command/help",          [](){ server.send(200, "text/plain", command("help", 1000)); input=""; });
  server.on("/command/version",       [](){ server.send(200, "text/plain", command("version", 1000)); input=""; });
  server.on("/command/status",        [](){ server.send(200, "text/plain", command("status", 2000)); input=""; });
  server.on("/command/stats",         [](){ server.send(200, "text/plain", command("stats", 1000)); input=""; });
  server.on("/command/runtime",       [](){ server.send(200, "text/plain", command("runtime", 1000)); input=""; });
  server.on("/command/bms",           [](){ server.send(200, "text/plain", command("bms", 1500)); input=""; });
  server.on("/command/bluetooth",     [](){ server.send(200, "text/plain", command("bluetooth", 1000)); input=""; });
  server.on("/command/sevcon",        [](){ server.send(200, "text/plain", command("sevcon", 1000)); input=""; });
  server.on("/command/chargers",      [](){ server.send(200, "text/plain", command("chargers", 1500)); input=""; });
  server.on("/command/inputs",        [](){ server.send(200, "text/plain", command("inputs", 1500)); input=""; });
  server.on("/command/outputs",       [](){ server.send(200, "text/plain", command("outputs", 1000)); input=""; });
  server.on("/command/dash",          [](){ server.send(200, "text/plain", command("dash", 1000)); input=""; });
  server.on("/command/obd",           [](){ server.send(200, "text/plain", command("obd", 1000)); input=""; });
  server.on("/command/errorlogdump",  [](){ server.send(200, "text/plain", command("errorlogdump", 1000)); input=""; });
  
#if GARAGE_DOOR_OPENER
  pinMode(0, OUTPUT);
  digitalWrite(0,HIGH);
  server.on("/button", [](){ server.send(200, "text/plain", String(digitalRead(0))); });
#endif

  server.begin();
  while(Serial.available() > 0) {
    Serial.read();
  }
}

void loop() {
  if(millis() > last_poll + POLLING_INTERVAL) { // Is it time to poll?
    while(Serial.available() > 0) {             // Flush the serial buffer so we can read only
      Serial.read();                            // what we want, not the misc data and events
    }                                           // that get dumped to serial output
    Serial.println("bms");
    sent_command = "bms";
    receiving = 1;
    last_poll = millis();
  }
  if(receiving){
    while (Serial.available() > 0){
      char in = Serial.read();
      input.concat(in);
      if(in == '>') { // End of a response from the bike
        if(sent_command == "bms") {
          parsebms();
          Serial.println("sevcon");
          sent_command = "sevcon";
          input="";
        } else if(sent_command == "sevcon") {
          parsesevcon();
          Serial.println("inputs");
          sent_command = "inputs";
          input="";
        } else if(sent_command == "inputs") {
          parseinputs();
          receiving=0;
          input="";
          break;
        }
      }
    }
  }

#if GARAGE_DOOR_OPENER
  if(!digitalRead(0)) { // Is anybody pressing the button to open the garage door?
    toggle();
  }
#endif

  server.handleClient(); // Handle HTTP requests
}

void parsebms() {
  int s=0;
  for (int i = 0; i < input.length(); i++) { // Step through the input string
    if (input[i] == '\n') {
      String line = input.substring(s, i);   // Parse it line by line
      if (line.startsWith("  - Pack SOC")) {
        int io = line.indexOf("%");
        soc = line.substring(io-3,io).toInt();
      } else if(line.startsWith("  - Fuel Gauge")) {
        int io = line.indexOf("%");
        fg = line.substring(io-3,io).toInt();
      } else if (line.startsWith("  - Pack Sum Voltage")) {
        int io = line.lastIndexOf("V");
        psv = line.substring(io-8,io).toFloat();
      } else if (line.startsWith("  - Sag-Adj Pack Sum Voltage")) {
        int io = line.lastIndexOf("V");
        sapsv = line.substring(io-8,io).toFloat();
      } else if (line.startsWith("  - Lowest Cell Voltage")) {
        int io = line.indexOf("m");
        lcv = line.substring(io-5,io).toInt();
      } else if (line.startsWith("  - Highest Cell Voltage")) {
        int io = line.indexOf("m");
        hcv = line.substring(io-5,io).toInt();
        break;
      }
      s=i+1; // Set the start of the next line after the newline char
    }
  } 
}

void parsesevcon() {
  int s=0;
  for (int i = 0; i < input.length(); i++) {
    if (input[i] == '\n') {
      String line = input.substring(s, i);
      if (line.startsWith("  - Motor Temp")) {
        int io = line.lastIndexOf("C");
        mt = line.substring(io-4,io).toInt();
      } else if(line.startsWith("  - Controller Temp")) {
        int io = line.lastIndexOf("C");
        ct = line.substring(io-4,io).toInt();
        break;
      }
      s=i+1;
    }
  } 
}

void parseinputs() {
  int s=0;
  for (int i = 0; i < input.length(); i++) {
    if (input[i] == '\n') {
      String line = input.substring(s, i);
      if (line.startsWith("  - Board Temp")) {
        int io = line.lastIndexOf("C");
        bt = line.substring(io-4,io).toInt();
      } else if(line.startsWith("  - Ambient Temp")) {
        int io = line.lastIndexOf("C");
        at = line.substring(io-4,io).toInt();
        break;
      }
      s=i+1;
    }
  } 
}


String command(String cmd, int timeout) { // Run a direct command from an HTTP client
  Serial.println(cmd);
  unsigned long start_time = millis();
  receiving=1;
  while(receiving){                       // Just busy loop in here for timeout ms
    while (Serial.available() > 0){
      char in = Serial.read();
      input.concat(in);
      if(in == '>') {
        receiving=0;
        break;
      }
    }
    if (millis() > start_time + timeout) { break; }
  }
  dprintln("read input: " + input);
  return input;
}

String make_json() { // Lame JSON creation
  String ret;
  ret += "{\n";
  ret += "  \"controller\": {\n";
#if GARAGE_DOOR_OPENER
  ret += "    \"button\": \"" + String(digitalRead(0)) + "\",\n";
#endif
  ret += "    \"ip\": \"" + WiFi.localIP().toString() + "\",\n";
  ret += "    \"uptime\": \"" + String(millis()) + "\"\n";
  ret += "  },\n";
  ret += "  \"zero\": {\n";
  ret += "    \"fg\": \"" + String(fg) + "\",\n";
  ret += "    \"soc\": \"" + String(soc) + "\",\n";
  ret += "    \"psv\": \"" + String(psv) + "\",\n";
  ret += "    \"sapsv\": \"" + String(sapsv) + "\",\n";
  ret += "    \"lcv\": \"" + String(lcv) + "\",\n";
  ret += "    \"hcv\": \"" + String(hcv) + "\",\n";
  ret += "    \"mt\": \"" + String(mt) + "\",\n";
  ret += "    \"ct\": \"" + String(ct) + "\",\n";
  ret += "    \"bt\": \"" + String(bt) + "\",\n";
  ret += "    \"at\": \"" + String(at) + "\"\n";
  ret += "  }\n";
  ret += "}";
  return ret;
}

#if GARAGE_DOOR_OPENER
const char *host = "10.0.0.3"; // IP address of garage door opener
void toggle() { // Hit the garage door: https://github.com/patfreeman/garage_door_closer
  if (millis() > last_toggle + 5000) { // 5s between button click actions
    last_toggle = millis();
    WiFiClient client;
    dprintln("button pressed");
    if (client.connect(host,80)) {
      client.print(String("GET /status HTTP/1.1\r\n") +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n" +
                 "Authorization: Basic base64_user_password\r\n" + // Update me
                 "\r\n");
      dprintln("Sent close request to garage door");
    }
    delay(150); // Prevent 499 return code
    client.stop();
  }
}
#endif
