//required for Uno WiFi Rev2
#include <SPI.h>
#include <WiFiNINA.h> 

//sensitive data is stored in the Secret tab/arduino_secrets.h
#include "arduino_secrets.h" 

char ssid[] = SECRET_SSID;        // network SSID (name)
char pass[] = SECRET_PASS;        // network password
int status = WL_IDLE_STATUS;

// Initialize the client class
WiFiSSLClient client;

char server[] = HOST_NAME;      // connection hostname
int port = 443;                 // connection port


void setup() {
  //Initialize serial monitor
  Serial.begin(9600);

  // initialize digital pin LED_BUILTIN as an output
  pinMode(LED_BUILTIN, OUTPUT);

  // connect to WiFi
  connectToNetwork();  

}


void loop() { 
  sendAlert();
  while(true);
}


void sendAlert(){
  Serial.print("Attempting to connet to host: ");
  Serial.println(HOST_NAME);

  if( client.connect(HOST_NAME, PORT)){
    Serial.println("connected to host.");
    // make a HTTPS request:
  } else {
    Serial.println("Connection failed.");
  }
  
}

void connectToNetwork(){
  Serial.print("Attempting to connect to Network SSID: ");
  Serial.println(ssid);

  // turn on amber led
  digitalWrite(LED_BUILTIN, HIGH);
 
  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {    
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);    
    // wait 5 seconds for connection
    delay(5000);

    if(status != WL_CONNECTED){
      Serial.println("Attempt failed, trying again...");
      // blink for half a second
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Connected to network.");
  Serial.println();
  printCurrentNet();
  printWiFiData();
}

void printWiFiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
