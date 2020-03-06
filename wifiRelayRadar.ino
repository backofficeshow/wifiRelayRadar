#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
//needed for wifi manager library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

char ipaddress[25] = "";
#define SETTINGS_RESET_PIN 14 // (Also Known as D5 on nodeMCU boards
#define radarPin 16  // The pin that the Radar Module is connected to
int radarValue = 0; // The current value of the Radar Pin
int previousRadarValue = 0; //Store the last value we received
unsigned long radarCounts = 0; // A counter that stores all the times the Radar was activated
unsigned long countsLastChecked = 0; // A counter that stores the value the last time the webpage was checked

#define relay1 5    //We will make this an array on the next iteration, as we may have 8 or 16 channel relay boards!
#define relay2 4
#define relay3 0
#define relay4 2
#define relayON 1   //We are inverting the outputs so active LOW
#define relayOFF 0  //Becuase of a limitatation on GPIO 0 and 2 pins
unsigned char relayValue1 = relayOFF;
unsigned char relayValue2 = relayOFF;
unsigned char relayValue3 = relayOFF;
unsigned char relayValue4 = relayOFF;

volatile unsigned long next; //ISR

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

// Variable to store the HTTP request
String header;

void allRelaysOff(){
  relayValue1 = relayOFF;
  relayValue2 = relayOFF;
  relayValue3 = relayOFF;
  relayValue4 = relayOFF;
}

void allRelaysOn(){
  relayValue1 = relayON;
  relayValue2 = relayON;
  relayValue3 = relayON;
  relayValue4 = relayON;
}

void writeRelayValues(){
  pinMode(relay1, relayValue1); 
  pinMode(relay2, relayValue2); 
  pinMode(relay3, relayValue3); 
  pinMode(relay4, relayValue4);  
}

void setup() {
  Serial.begin(115200);
  delay(10);

  //Setup Relay Pins
  pinMode(relay1, OUTPUT); 
  pinMode(relay2, OUTPUT); 
  pinMode(relay3, OUTPUT); 
  pinMode(relay4, OUTPUT); 
  allRelaysOff();
  writeRelayValues();

  //WIFI reset on powerup mode
  pinMode(SETTINGS_RESET_PIN, INPUT_PULLUP);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  //exit after config instead of connecting
  wifiManager.setBreakAfterConfig(false);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //if it sees the reset pin pulled low, it loads the AP immediately
  if ( digitalRead(SETTINGS_RESET_PIN) == LOW ) {
    Serial.println("Resetting Settings.");
    wifiManager.resetSettings();
  }
  
  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" with password "password"
  //and goes into a blocking loop awaiting configuration 
  if (!wifiManager.autoConnect("TBOSNode", "12345678")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  sprintf(ipaddress, "%s", (char*) WiFi.localIP().toString().c_str());

  Serial.println("All good, lets go."); 
  // start a server
  server.begin();
  Serial.println("Server started");
  delay(3000);   
}

void loop() {
  delay(100);


  // Read the value of the sensor
  radarValue = digitalRead(radarPin);
  if (radarValue != previousRadarValue)
  {
    if (radarValue == 1)
    {
      radarCounts++; // Only count activations and not de-activations
      Serial.print("Trigger Detected "); 
      Serial.println(radarCounts); 
    }
    previousRadarValue = radarValue;
  }
 
  // Check if a client has connected
  WiFiClient client = server.available();

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><style>body{background-color:");
            
            //Change the background colour based on sense!
            if (radarValue == 1) 
            {
              client.println("red;");              
            }            
            client.println("}</style></head>");
            
            // Web Page Heading
            client.println("<body>");

            client.println("Relay Status:<br>");  
            client.print("Relay 1: ");
            client.println(relayValue1);
            client.print("<a href=\"/relayon1t\">on timed</a>|<a href=\"/relayon1\">on</a>|<a href=\"/relayoff1\">off</a><br>Relay 2: ");
            client.println(relayValue2);
            client.print("<a href=\"/relayon2t\">on timed</a>|<a href=\"/relayon2\">on</a>|<a href=\"/relayoff2\">off</a><br>Relay 3: ");
            client.println(relayValue3);
            client.print("<a href=\"/relayon3t\">on timed</a>|<a href=\"/relayon3\">on</a>|<a href=\"/relayoff3\">off</a><br>Relay 4: ");
            client.println(relayValue4);
            
            client.println("<a href=\"/relayon4t\">on timed</a>|<a href=\"/relayon4\">on</a>|<a href=\"/relayoff4\">off</a><br><a href=\"/relayallon\">All On</a>|<a href=\"/relayalloff\">All Off</a><br>Radar Status:");
                        
            if (radarValue == 0) 
            {
              client.println("No");  
            }

            client.println(" person detected right now - there were");
            client.println(radarCounts - countsLastChecked);
            client.println("activations since you last checked this page -");
            client.println(radarCounts);
            client.println("total<br><br><b>TBOSNode Relay Radar- backofficeshow.com</b>");
            client.println("Click <a href=\"/R\">here</a> to reset the trigger counters");
            client.println("</body></html>");


            countsLastChecked = radarCounts; // Update our counter so we can see how many activations happened in-between checks

            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        
        if (currentLine.endsWith("GET /R")) {
          Serial.println("User reset the counters"); 
          radarCounts = 0;  // Reset the counters
          countsLastChecked = 0;
        }

        if (currentLine.endsWith("GET /relayon1")) {
          Serial.println("Relay 1 on"); 
          relayValue1=relayON;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayon2")) {
          Serial.println("Relay 2 on"); 
          relayValue2=relayON;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayon3")) {
          Serial.println("Relay 3 on"); 
          relayValue3=relayON;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayon4")) {
          Serial.println("Relay 4 on"); 
          relayValue4=relayON;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayon1t")) {
          Serial.println("Relay 1 on"); 
          relayValue1=relayON;
          writeRelayValues();
          delay(2000);
          relayValue1=relayOFF;
          writeRelayValues();                    
        }
        if (currentLine.endsWith("GET /relayon2t")) {
          Serial.println("Relay 2 on"); 
          relayValue2=relayON;
          writeRelayValues();
          delay(2000);
          relayValue2=relayOFF;
          writeRelayValues();          
        }
        if (currentLine.endsWith("GET /relayon3t")) {
          Serial.println("Relay 3 on"); 
          relayValue3=relayON;
          writeRelayValues();
          delay(2000);
          relayValue3=relayOFF;
          writeRelayValues();          
        }
        if (currentLine.endsWith("GET /relayon4t")) {
          Serial.println("Relay 4 on"); 
          relayValue4=relayON;
          writeRelayValues();
          delay(2000);
          relayValue4=relayOFF;
          writeRelayValues();          
        }                          
        if (currentLine.endsWith("GET /relayoff1")) {
          Serial.println("Relay 1 off"); 
          relayValue1=relayOFF;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayoff2")) {
          Serial.println("Relay 2 off"); 
          relayValue2=relayOFF;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayoff3")) {
          Serial.println("Relay 3 off"); 
          relayValue3=relayOFF;
          writeRelayValues();
        }
        if (currentLine.endsWith("GET /relayoff4")) {
          Serial.println("Relay 4 off"); 
          relayValue4=relayOFF;
          writeRelayValues();
        }         
        if (currentLine.endsWith("GET /relayallon")) {
          Serial.println("All Relays On"); 
          allRelaysOn();
          writeRelayValues();
        }           
        if (currentLine.endsWith("GET /relayalloff")) {
          Serial.println("All Relays Off"); 
          allRelaysOff();
          writeRelayValues();
        }                   
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
  }

}
