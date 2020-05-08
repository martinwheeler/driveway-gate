
/**
 * This project controls two DC motors to open and close a driveway gate. It also controls some lights which will be turned on during operation of the gate.
 */ 

using namespace std; 

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecureBearSSL.h>
#include "config.h"
#include "Utils.h"

const int IS_DEBUG = 0;
const int IS_LIGHT_ENABLED = 1;
const int IS_APP_ENABLED = 1;
const String VERSION = "1.0.0";

HTTPClient http;
WiFiClient wifiClient;
WiFiServer server(80);
String clientRequest, jsonPayload;
bool hasSeenNewline;
char characterRead;
std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

// The pins below are the GPIO numbers found on the back of the Wemos D1 R2 board. Which is why the digital pin number is also mentioned in comments.
const int LEFT_MOTOR = 2;           // Cable colour: BLUE, Digital Pin: D9
const int RIGHT_MOTOR = 13;         // Cable colour: WHITE, Digital Pin: D7
const int GATE_DIRECTION = 14;      // Cable colour: BROWN, Digital Pin: D5
const int TRIGGER_PIN = 5;          // Cable colour: BLACK, Digital Pin: D3
const int LIGHT_PIN = 4;            // Cable colour: ORANGE, Digital Pin: D4

int openTrigger = LOW, previousConnectionStatus, lightOffTime;
bool hasGateOpened = false, gateOpening = false, gateClosing = false, attemptingWifiConnection = false, firstConnection = true, lightsOn = false;

const int OPEN_DURATION = 70000; // 70 seconds
const int LEFT_CLOSE_DELAY = 4750; // 4.75 seconds
const int RIGHT_OPEN_DELAY = 350; // 0.35 seconds
const int MOVING_DURATION = 12000; // 12 seconds
const int CONNECTION_TIMEOUT = 30000; // 30 seconds
const int LIGHT_OFF_TIMEOUT = 30000; // 30 seconds

unsigned long currentTime = 0, gateOpeningTime = 0, gateClosingTime = 0, connectionStartTime = 0;

vector<String> unsentMessages;

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  connectionStartTime = millis();
  WiFi.begin(SSID, SECRET);  
}

void printWifiData(bool reconnected = false) {
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

  String wifiPayload = "*=======================================================================*\r\n";
  wifiPayload += "*Network Connection* :computer::signal_strength:\r\n";
  wifiPayload += reconnected ? "Reconnected\r\n" : "";
  wifiPayload += "\r\nSSID: " + String(SSID);
  wifiPayload += "\r\nIP: " + ipStr;
  wifiPayload += "\r\nSignal: " + String(WiFi.RSSI()) + " dBm";
  wifiPayload += "\r\n*=======================================================================*";

  logMessage(wifiPayload);
}

void printInformation() {
  String informationPayload = "*=======================================================================*\r\n";
  informationPayload += "*Config* :bulb:\r\n";
  informationPayload += "\r\nVersion: " + VERSION;
  informationPayload += "\r\nLights: ";
  informationPayload += (IS_LIGHT_ENABLED) ? "Yes" : "No";
  informationPayload += "\r\nApp: ";
  informationPayload += (IS_APP_ENABLED) ? "Yes" : "No";
  informationPayload += "\r\nClose Delay: " + String(OPEN_DURATION / 1000) + " seconds";
  informationPayload += "\r\nLight Off Delay: " + String(LIGHT_OFF_TIMEOUT / 1000) + " seconds";
  informationPayload += "\r\n*=======================================================================*";

  logMessage(informationPayload);
}

/**
 * Logs out a message to the serial monitor or sends it to a Slack channel.
 * 
 * @param String message - The message that will be sent.
 */
void logMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    int responseCode = 0;
    client->setFingerprint(SLACK_HTTPS_FINGERPRINT);
    
    StaticJsonDocument<512> jsonDoc;
 
    jsonDoc["text"] = message;
    jsonDoc["channel"] = "#66-broadoak-gate";
    jsonDoc["username"] = "GateBot";
    jsonDoc["icon_emoji"] = ":car:";

    if (IS_DEBUG) {
      int messageLength = measureJson(jsonDoc);
  
      Serial.print("Payload Size: ");
      Serial.println(messageLength);  
    }
    
    char slackPayload[512];
    serializeJson(jsonDoc, slackPayload);
    
    if (IS_DEBUG) {
      Serial.println(slackPayload); 
    }
    
    http.begin(*client, SLACK_WEBHOOK);
    http.addHeader("Content-Type", "application/json");
    responseCode = http.POST(slackPayload);

    if (IS_DEBUG && responseCode < 0) {
      Serial.printf("[HTTPS] POST... failed, error: %s\n", http.errorToString(responseCode).c_str());
    }
    
    http.end();
  } else {
    if (IS_DEBUG) {
     Serial.println("\r\nSaving message:\r\n"); 
    }
    
    unsentMessages.push_back(message);
  }

  if (IS_DEBUG) {
    Serial.println(message);
  }
}

bool startedOpening = false, startedOpeningRight = false;

void openGate() {
  currentTime = millis() - gateOpeningTime;

  if (!startedOpening) {
    logMessage("The front gate is beginning to open.");
    digitalWrite(GATE_DIRECTION , HIGH);
    digitalWrite(LEFT_MOTOR , HIGH);
    startedOpening = true;
  }

  if (startedOpening && currentTime > RIGHT_OPEN_DELAY && !startedOpeningRight) {
    digitalWrite(RIGHT_MOTOR , HIGH);
    startedOpeningRight = true;
  }

  if (currentTime > MOVING_DURATION && startedOpeningRight) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    digitalWrite(GATE_DIRECTION , LOW);
    logMessage("The front gate is open.");
    hasGateOpened = true;
    gateOpening = false;
  }
}

bool startedClosing = false, startedClosingLeft = false;

void closeGate () {
  currentTime = millis() - gateClosingTime;

  if (!startedClosing) {
    logMessage("The front gate is beginning to close.");
    digitalWrite(RIGHT_MOTOR , HIGH);
    startedClosing = true;
  }

  if (startedClosing && currentTime > LEFT_CLOSE_DELAY && !startedClosingLeft) {
    digitalWrite(LEFT_MOTOR , HIGH);
    startedClosingLeft = true;
  }

  if (currentTime > MOVING_DURATION && startedClosingLeft) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    logMessage("The front gate is closed.");
    hasGateOpened = false;
    gateClosing = false;
  }
}

void handleLights () {
  if (gateOpening && !startedOpening) {
    logMessage("Turning on the lights.");
    digitalWrite(LIGHT_PIN, HIGH); // Turn on the lights
    lightsOn = true;
  }

  // Always update this while the gate is closing, once closed we then count down from that time.
  if (gateClosing && (lightsOn && !gateOpening)) {
    lightOffTime = millis();
  }

  if (!hasGateOpened && ((millis() - lightOffTime) > LIGHT_OFF_TIMEOUT) && lightsOn && !gateOpening) {
    digitalWrite(LIGHT_PIN, LOW); // Turn off the lights
    logMessage("Turning off the lights.");
    lightsOn = false;
  }
}

void setup(){
  if (IS_DEBUG) {
    Serial.begin(9600);
    while (!Serial) {
      return; // wait for serial port to connect. Required for debugging.
    }
  }
  
  if (IS_DEBUG) {
    Serial.println();
    Serial.println("DEBUG MODE");
    Serial.println();
  }

  connectWifi();
  server.begin();
  
  pinMode(RIGHT_MOTOR, OUTPUT);
  pinMode(LEFT_MOTOR, OUTPUT);
  pinMode(GATE_DIRECTION, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  digitalWrite(GATE_DIRECTION, LOW);
  digitalWrite(LEFT_MOTOR, LOW);
  digitalWrite(RIGHT_MOTOR, LOW);
  digitalWrite(LIGHT_PIN, LOW);
}

void handleUnsentMessages () {
  if (unsentMessages.size() > 0 && WiFi.status() == WL_CONNECTED) {    
    auto it = unsentMessages.begin();
    while (it != unsentMessages.end()) {
      logMessage(*it);
      it = unsentMessages.erase(it);
    }
  }
}

void handleGateTrigger () {
  if (openTrigger == LOW && !hasGateOpened && !gateOpening) {
    startedOpening = false;
    startedOpeningRight = false;
    gateOpening = true;
    gateOpeningTime = millis();
  }
  
  if (openTrigger == HIGH && hasGateOpened && !gateClosing && (millis() - gateOpeningTime > OPEN_DURATION)) {
    startedClosing = false;
    startedClosingLeft = false;
    gateClosing = true;
    gateClosingTime = millis();
  }

  if (openTrigger == LOW && gateClosing && (millis() - gateOpeningTime > 100)) {
    startedOpening = false;
    startedOpeningRight = false;
    gateOpening = true;
    gateClosing = false;
    gateOpeningTime = millis();
  }
}

String getGateStatus () {

  if (hasGateOpened && gateClosing) {
    return "closing";
  }

  if (hasGateOpened && !gateClosing && !gateOpening) {
    return "open";
  }

  if (!hasGateOpened && gateOpening) {
    return "opening";
  }

  if (!hasGateOpened && !gateClosing && !gateOpening) {
    return "closed";
  }
  
  return "unsure";
}

void handleAppRequests () {
  wifiClient = server.available();

  if (!wifiClient) {
    return;
  }

  clientRequest = "";
  hasSeenNewline = false;
  while (wifiClient.connected()) {
    if (wifiClient.available()) {
      characterRead = wifiClient.read();
      if (hasSeenNewline && characterRead != '\r') {
        hasSeenNewline = false;
      }

      if (hasSeenNewline) {
        break;
      }

      clientRequest += characterRead;

      if (characterRead == '\n') {
        hasSeenNewline = true;
      }
    }
  }

  if (clientRequest.indexOf("OPTIONS") != -1) {
    wifiClient.print("HTTP/2 200 OK\r\n\r\n");
    return;
  }

  if (
    clientRequest.indexOf("POST") != -1 &&
    clientRequest.indexOf(AUTHORIZATION_VALUE) != -1
  ) {
    if (clientRequest.indexOf("/status") != -1) {
      jsonPayload = "{";
      jsonPayload += "  \"success\": true";
      jsonPayload += ",  \"wifiSignal\": \"" + String(WiFi.RSSI()) + "\"";
      jsonPayload += ",  \"gate\": \"" + getGateStatus() + "\"";
      jsonPayload += ",  \"light\": \"" + String(lightsOn ? "on" : "off") + "\"";
      jsonPayload += "}";

      wifiClient.print("HTTP/2 200 OK\r\nContent-type: application/json\r\n\r\n");
      wifiClient.print(jsonPayload);
    } else if (clientRequest.indexOf("/trigger") != -1) {
      openTrigger = LOW;
      handleGateTrigger();
      
      jsonPayload = "{";
      jsonPayload += "  \"success\": true";
      jsonPayload += "}";

      wifiClient.print("HTTP/2 200 OK\r\nContent-type: application/json\r\n\r\n");
      wifiClient.print(jsonPayload);
    } else {
      wifiClient.print("HTTP/2 404 OK\r\n\r\n");
    }
  } else {
    wifiClient.print("HTTP/2 401 OK\r\n\r\n");
  }
}

void loop(){
  openTrigger = digitalRead(TRIGGER_PIN);

  if (previousConnectionStatus != WL_CONNECTED && WiFi.status() == WL_CONNECTED && !firstConnection) {
    printWifiData(true);
  }
  previousConnectionStatus = WiFi.status();

  if (firstConnection && (WiFi.status() == WL_CONNECTED)) {
    printWifiData();
    printInformation();
    firstConnection = false;
  }

  if (WiFi.status() != WL_CONNECTED && (millis() - connectionStartTime > CONNECTION_TIMEOUT)) {
    connectWifi();
  }

  handleGateTrigger();

  if (IS_LIGHT_ENABLED) {
    handleLights(); 
  }

  if (gateOpening) {
    openGate();
  }

  if (gateClosing) {
    closeGate();
  }

  handleUnsentMessages();
  if (IS_APP_ENABLED) {
    handleAppRequests(); 
  }
}
