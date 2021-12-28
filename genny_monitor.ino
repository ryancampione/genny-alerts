//required for Uno WiFi Rev2
#include <SPI.h>
#include <WiFiNINA.h> 
#include <ArduinoECCX08.h>
#include <utility/ECCX08JWS.h>
#include <ArduinoMqttClient.h>
#include <Arduino_JSON.h>

//sensitive data is stored in the Secret tab/arduino_secrets.h
#include "arduino_secrets.h" 

// WiFi network details
const char ssid[] = NETWORK_SSID;
const char pass[] = NETWORK_PASS;

// GCP information
const char projectId[]   = SECRET_PROJECT_ID;
const char cloudRegion[] = SECRET_CLOUD_REGION;
const char registryId[]  = SECRET_REGISTRY_ID;
const String deviceId    = SECRET_DEVICE_ID;

// MQTT host connection information
const char broker[] = HOST_NAME;
const int port = 8883;

// circuit contact
const int analogPin = A0;

// Initialize client classes
WiFiSSLClient client;
MqttClient    mqttClient(client);

// temp variables 
int voltage = 0;
bool isGenOn = false;
bool msgOnConnection = false;
unsigned long lastMillis = 0;


void setup() {
  //Initialize serial monitor
  Serial.begin(9600);

  // initialize digital pin LED_BUILTIN as an output
  pinMode(LED_BUILTIN, OUTPUT);

  // ensure ECCX08 is present for secret key
  if (!ECCX08.begin()) {
    Serial.println("No ECCX08 present!");
    while (true);
  }

  // Calculate and set the client id used for MQTT
  String clientId = calculateClientId();
  Serial.print("GCP Client ID: ");
  Serial.println(clientId);
  mqttClient.setId(clientId);

  // Set the message callback, this function is
  // called when the MQTTClient receives a message
  mqttClient.onMessage(onMessageReceived);
}


void loop() { 
  // connect to WiFi network
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // connect to MQTT client
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // poll for new MQTT messages and send keep alives
  mqttClient.poll();

  // read voltage every 30 seconds
  if (millis() - lastMillis > 30000) {
    lastMillis = millis();

    // get voltage from analog pin
    voltage = analogRead(analogPin);
    Serial.print("Voltage: ");
    Serial.println(voltage);
    Serial.print("Generator status: ");
    Serial.println(isGenOn);

    // check for change in state
    // voltage detected and genarator was off
    if (voltage > 1 && !isGenOn){
      isGenOn = true;  
      publishEventMessage();
    } 
    // voltage not decteded and genarator was on
    else if (voltage <= 1 && isGenOn){
      isGenOn = false;  
      publishEventMessage();
    }
    // config to send msg
    else if (msgOnConnection){
      publishEventMessage();
    }
    
    // send device stats
    publishDeviceMessage();
  }
}

/* 
 *  Network / WiFi connection and info
 */
void connectWiFi(){
  Serial.println();
  Serial.print("Attempting to connect to Network SSID: ");
  Serial.println(ssid);

  // turn on amber led
  digitalWrite(LED_BUILTIN, HIGH);
 
  // attempt to connect to WiFi network:
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {       
    // wait 5 seconds for connection
    delay(5000);
    
    Serial.println("Attempt failed, trying again...");
    blink();
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Connected to network.");
  printWiFiData();
}

void printWiFiData() {
  // print your WiFi shield's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  Serial.print("Signal strength (RSSI):");
  Serial.println(WiFi.RSSI());

  // print the encryption type:
  Serial.print("Encryption Type:");
  Serial.println(WiFi.encryptionType(), HEX);
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

/* 
 *  MQTT connection
 */
void connectMQTT() {
  Serial.println();
  Serial.print("Attempting to connect to MQTT broker: ");
  Serial.println(broker);

  while (!mqttClient.connected()) {
    // Calculate the JWT and assign it as the password
    String jwt = calculateJWT();

    mqttClient.setUsernamePassword("", jwt);

    if (!mqttClient.connect(broker, port)) {
      // failed, retry
      Serial.println("Attempt failed, trying again...");
      blink();
    }
  }
  Serial.println("You're connected to the MQTT broker");
  digitalWrite(LED_BUILTIN, LOW);

  // subscribe to topics
  mqttClient.subscribe("/devices/" + deviceId + "/config", 1);
  mqttClient.subscribe("/devices/" + deviceId + "/commands/#");
}

String calculateClientId() {
  String clientId = "";
  // generate GCP client id
  // Format: projects/{project-id}/locations/{cloud-region}/registries/{registry-id}/devices/{device-id}
  clientId += "projects/";
  clientId += projectId;
  clientId += "/locations/";
  clientId += cloudRegion;
  clientId += "/registries/";
  clientId += registryId;
  clientId += "/devices/";
  clientId += deviceId;

  return clientId;
}

String calculateJWT() {
  unsigned long now = getTime();
  
  // calculate the JWT, based on:
  //   https://cloud.google.com/iot/docs/how-tos/credentials/jwts
  JSONVar jwtHeader;
  JSONVar jwtClaim;

  jwtHeader["alg"] = "ES256";
  jwtHeader["typ"] = "JWT";

  jwtClaim["aud"] = projectId;
  jwtClaim["iat"] = now;
  jwtClaim["exp"] = now + (24L * 60L * 60L); // expires in 24 hours 

  return ECCX08JWS.sign(0, JSON.stringify(jwtHeader), JSON.stringify(jwtClaim));
}

/* 
 *  IoT messages via MQTT
 */
bool getConfigValue(String configTxt, String parameter, String defaultValue = "false"){
  String paramValue = defaultValue;;
  int startIndex = configTxt.indexOf(parameter);
  int stopIndex = startIndex;

  if(startIndex > -1){
    stopIndex = configTxt.indexOf("\r", startIndex);
    Serial.print("Config parameter " + parameter + " found with a value of '");
    // ensure parameter value exists
    paramValue = configTxt.substring(startIndex + parameter.length(), stopIndex);
  } else {
    Serial.print("Could not find parameter " + parameter + ", using default value of '");
  }
  paramValue.toLowerCase();
  Serial.println(paramValue + "'");

  if (paramValue.equals("true")){
    return true;
  } else {
    return false;
  }
}
 
void publishEventMessage() {
  float v = (float)voltage / (float)250;
  
  Serial.println("Publishing device event message");
  
  // send message, the Print interface can be used to set the message contents
  mqttClient.beginMessage("/devices/" + deviceId + "/events");
  mqttClient.print("Generator status:");
  if (isGenOn) {
    mqttClient.println("ON");
  } else {
    mqttClient.println("OFF");
  }
  mqttClient.print("Device voltage:");
  mqttClient.println(v, 2);
  mqttClient.endMessage();

  // reset to default
  msgOnConnection = false;
}

void publishDeviceMessage() {
  Serial.println("Publishing device state message");

  // send device state message
  mqttClient.beginMessage("/devices/" + deviceId + "/state");
  mqttClient.print("WiFi Network:");
  mqttClient.println(WiFi.SSID());
  mqttClient.print("Signal Stregnth:");
  mqttClient.println(WiFi.RSSI());
  mqttClient.print("IP:");
  mqttClient.println(WiFi.localIP());
  mqttClient.print("Voltage:");
  mqttClient.println(voltage);
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  String topic = mqttClient.messageTopic();
  String msg = "";
  
  Serial.println();
  // received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(topic);
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    msg += (char)mqttClient.read();
  }
  Serial.println(msg);

  // check for config
  if(topic.indexOf(deviceId + "/config") > -1){
    Serial.println("Config message found...");
    // check for parameters
    msgOnConnection = getConfigValue(msg,"MsgOnConnection=","false");
  }
  Serial.println();
}

/* 
 *  Utility
 */
unsigned long getTime() {
  // get the current time from the WiFi module
  return WiFi.getTime();
}

void blink() {
  // blink the led light twice
  for (int i = 0; i <= 6; i++){
    if(digitalRead(LED_BUILTIN) == HIGH) {
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    delay(500);  
  }
}
