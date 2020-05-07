
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

const int IS_DEBUG = 1;
const int IS_LIGHT_ENABLE = 1;
const String VERSION = "1.0.0";

HTTPClient http;
std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

// The pins below are the GPIO numbers found on the back of the Wemos D1 R2 board. Which is why the digital pin number is also mentioned in comments.
const int LEFT_MOTOR = 2;           // Cable colour: BLUE, Digital Pin: D9
const int RIGHT_MOTOR = 13;         // Cable colour: WHITE, Digital Pin: D7
const int GATE_DIRECTION = 14;      // Cable colour: BROWN, Digital Pin: D5
const int TRIGGER_PIN = 5;          // Cable colour: BLACK, Digital Pin: D3
const int LIGHT_PIN = 4;            // Cable colour: ORANGE, Digital Pin: D4

int openTrigger = LOW;
bool hasGateOpened = false, gateOpening = false, gateClosing = false, attemptingWifiConnection = false;

const int OPEN_DURATION = 70000; // 70 seconds
const int LEFT_CLOSE_DELAY = 4750; // 4.75 seconds
const int RIGHT_OPEN_DELAY = 350; // 0.35 seconds
const int MOVING_DURATION = 12000; // 12 seconds

unsigned long currentTime = 0, gateOpeningTime = 0, gateClosingTime = 0;

vector<String> unsentMessages;

void connectWifi(int retries = 200) {
  int tries = 0;
  attemptingWifiConnection = true;

  WiFi.begin(SSID, SECRET);

  if (IS_DEBUG) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.print(SSID);
    Serial.println();
    Serial.print("Connecting");
  }

  while (WiFi.status() != WL_CONNECTED) {
    if (IS_DEBUG) {
      Serial.print(".");
    }
    
    delay(500);
 
    tries++;
    if (tries >= retries) {
      attemptingWifiConnection = false;
      break;
    }
  }

  if (tries >= retries) {
    attemptingWifiConnection = false;
    return;
  }

  if (IS_DEBUG) {
    Serial.println("\r\n");
  }
  
  printWifiData();
}

void printWifiData() {
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

  String wifiPayload = "Network Connection :computer::signal_strength:\r\n";
  wifiPayload += "*--------------------------------------------*\r\n";
  wifiPayload += "SSID: ";
  wifiPayload += SSID;
  wifiPayload += "\r\n";
  wifiPayload += "IP: ";
  wifiPayload += ipStr;
  wifiPayload += "\r\n";
  wifiPayload += "Signal: " + String(WiFi.RSSI()) + " dBm";
  wifiPayload += "\r\n*--------------------------------------------*";

  logMessage(wifiPayload);
}

/**
 * Logs out a message to the serial monitor or sends it to a Slack channel.
 * 
 * @param String message - The message that will be sent.
 */
void logMessage(String message) {
  if (IS_DEBUG) {
    Serial.println(message);
  }

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
  
      Serial.print("Size: ");
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

    if (responseCode < 0 && IS_DEBUG) {
      Serial.printf("[HTTPS] POST... failed, error: %s\n", http.errorToString(responseCode).c_str());
    }
    
    http.end();
  } else {
    Serial.println("\r\n Saving unsent message \r\n");
    unsentMessages.push_back(message);
  }
}

void printInformation() {
  String informationPayload = "Config :bulb:\r\n";
  informationPayload += "*--------------------------------------------*\r\n";
  informationPayload += "\r\nVersion: " + VERSION;
  informationPayload += "\r\nLights: ";
  informationPayload += (IS_LIGHT_ENABLE) ? "Yes" : "No";
  informationPayload += "\r\nClose Delay: " + String(OPEN_DURATION / 1000) + "seconds";
  informationPayload += "\r\n*--------------------------------------------*";

  logMessage(informationPayload);
}

void openGate() {
  currentTime = millis() - gateOpeningTime;

  if (currentTime < 100) {
    digitalWrite(GATE_DIRECTION , HIGH);
    digitalWrite(LEFT_MOTOR , HIGH);

    if (IS_LIGHT_ENABLE) {
      digitalWrite(LIGHT_PIN, HIGH); // Turn on the lights now that the gates are opening
    }

    logMessage("The front gate is beginning to open.");
  }

  if (currentTime > RIGHT_OPEN_DELAY && currentTime < (RIGHT_OPEN_DELAY + 100)) {
    digitalWrite(RIGHT_MOTOR , HIGH);
  }

  if (currentTime > MOVING_DURATION) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    digitalWrite(GATE_DIRECTION , LOW);
    hasGateOpened = true;
    gateOpening = false;
  }
}

void closeGate () {  
  currentTime = millis() - gateClosingTime;

  // Begin closing the gate
  if (currentTime < 100) {
    digitalWrite(GATE_DIRECTION , LOW);
    digitalWrite(RIGHT_MOTOR , HIGH);

    logMessage("The front gate is beginning to close.");
  }

  if (currentTime > LEFT_CLOSE_DELAY && currentTime < (LEFT_CLOSE_DELAY + 100)) {
    digitalWrite(LEFT_MOTOR , HIGH); 
  }

  if (currentTime > MOVING_DURATION) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    hasGateOpened = false;
    gateClosing = false;
  }
}

void handleLights () {

  // TODO: Toggle lights off or on with a time delay
  
}

void setup(){
  if (IS_DEBUG) {
    Serial.begin(9600); // open the serial port at 9600 bps:
    while (!Serial) {
      return; // wait for serial port to connect. Required for debugging.
    }
  }
  
  if (IS_DEBUG) {
    Serial.println();
    Serial.println("DEBUG MODE");
    Serial.println();
  }

  while (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
  
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
  
void loop(){
  openTrigger = digitalRead(TRIGGER_PIN);

  if (!attemptingWifiConnection && (WiFi.status() != WL_CONNECTED)) {
    connectWifi();
  }

  if (unsentMessages.size() > 0 && WiFi.status() == WL_CONNECTED) {
    Serial.println("Sending unsent messages");
    
    for (const String &message : unsentMessages) {
      logMessage(message);
    }
  }

  if (openTrigger == LOW && !hasGateOpened && !gateOpening) {
    gateOpening = true;
    gateOpeningTime = millis();
  }
  
  if (openTrigger == HIGH && hasGateOpened && !gateClosing && (millis() - gateOpeningTime > OPEN_DURATION)) {
    gateClosing = true;
    gateClosingTime = millis();
  }

  if (openTrigger == LOW && gateClosing && (millis() - gateOpeningTime > 100)) {
    gateOpening = true;
    gateClosing = false;
    gateOpeningTime = millis();
  }

  handleLights();

  if (gateOpening) {
    openGate();
    return;
  }

  if (gateClosing) {
    closeGate();
    return;
  }
}
