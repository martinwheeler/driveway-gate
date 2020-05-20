#include <WiFiServerSecure.h>
#include <WiFiClientSecure.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiType.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFiAP.h>
#include <WiFiClient.h>
#include <BearSSLHelpers.h>
#include <WiFiServer.h>
#include <ESP8266WiFiScan.h>
#include <WiFiServerSecureBearSSL.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiSTA.h>
#include <WiFiClientSecureAxTLS.h>
#include <WiFiServerSecureAxTLS.h>

#include <ESP8266HTTPClient.h>


/**
 * This project controls two DC motors to open and close a driveway gate. It also controls some lights which will be turned on during operation of the gate.
 */
 
#include "Utils.h"

const int IS_DEBUG = 0;
const int IS_LIGHT_ENABLE = 1;
const String VERSION = "1.0.0";

// The pins below are the GPIO numbers found on the back of the Wemos D1 R2 board. Which is why the digital pin number is also mentioned in comments.
const int LEFT_MOTOR = 2;           // Cable colour: BLUE, Digital Pin: D9
const int RIGHT_MOTOR = 13;         // Cable colour: WHITE, Digital Pin: D7
const int GATE_DIRECTION = 14;      // Cable colour: BROWN, Digital Pin: D5
const int TRIGGER_PIN = 5;          // Cable colour: BLACK, Digital Pin: D3
const int LIGHT_PIN = 4;            // Cable colour: ORANGE, Digital Pin: D4

int openTrigger = LOW;
bool hasGateOpened = false, gateOpening = false, gateClosing = false;

const int OPEN_DURATION = 70000; // 70 seconds
const int LEFT_CLOSE_DELAY = 4750; // 4.75 seconds
const int RIGHT_OPEN_DELAY = 350; // 0.35 seconds
const int MOVING_DURATION = 12000; // 12 seconds

unsigned long currentTime = 0, gateOpeningTime = 0, gateClosingTime = 0;

// Wifi communication
const char* SSID = "G-Spot";
const char* SECRET = "evenstar078";
const String SLACK_WEBHOOK = "https://hooks.slack.com/services/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"; // MARTIN WHEELER SLACK SPACE
const String HTTPS_FINGERPRINT = "C1:0D:53:49:D2:3E:E5:2B:A2:61:D5:9E:6F:99:0D:3D:FD:8B:B2:B3"; // HTTPS Fingerprint SHA1 that matches with the HTTPS certificate from the slack hook address
HTTPClient http;

void connectWifi(int retries = 5) {
  int tries = 0;

  WiFi.begin(SSID, SECRET);

  if (IS_DEBUG) {
    Serial.println("Attempting to connect to SSID: ");
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
      break;
    }
  }

  if (tries >= retries) {
    return;
  }

  if (IS_DEBUG) {
    Serial.println();
  }
  printWifiData();
}

void printWifiData() {
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

  String wifiPayload = "Connectivity :computer:\r\n";
  wifiPayload += "*--------------------------------------------*\r\n";
  wifiPayload += "Connected to SSID: ";
  wifiPayload += SSID;
  wifiPayload += "\r\n";
  wifiPayload += "IP Address: ";
  wifiPayload += ipStr;
  wifiPayload += "\r\n";
  wifiPayload += "Signal Strength: " + String(WiFi.RSSI()) + " dBm";
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
  } else {
    http.begin(SLACK_WEBHOOK, HTTPS_FINGERPRINT);
    http.POST("{ \"text\": \"" + message + "\", \"channel\": \"#66-broadoak-gate\", \"username\": \"GateBot\", \"icon_emoji\": \":car:\" }");
    http.end();
  }
}

void printInformation() {
  String informationPayload = "Config :bulb:\r\n";
  informationPayload += "*--------------------------------------------*\r\n";
  informationPayload += "\r\nVersion: " + VERSION;
  informationPayload += "\r\nLights: ";
  informationPayload += (IS_LIGHT_ENABLE) ? "Yes" : "No";
  informationPayload += "\r\nClose Delay: " + String(OPEN_DURATION / 1000);
  informationPayload += "\r\n*--------------------------------------------*";

  logMessage(informationPayload);
}

void openGate() {
  if (IS_DEBUG) {
    Serial.println("OPEN");
  }
  
  if (IS_LIGHT_ENABLE) {
    digitalWrite(LIGHT_PIN, HIGH); // Turn on the lights now that the gates are opening
  }

  currentTime = millis() - gateOpeningTime;

  if (currentTime < 100) {
    digitalWrite(GATE_DIRECTION , HIGH);
    digitalWrite(LEFT_MOTOR , HIGH);
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
  if (IS_DEBUG) {
    Serial.println("CLOSE");
  }
  
  currentTime = millis() - gateClosingTime;

  // Begin closing the gate
  if (currentTime < 100) {
    digitalWrite(GATE_DIRECTION , LOW);
    digitalWrite(RIGHT_MOTOR , HIGH);  
  }

  if (currentTime > LEFT_CLOSE_DELAY && currentTime < (LEFT_CLOSE_DELAY + 100)) {
    digitalWrite(LEFT_MOTOR , HIGH); 
  }

  if (currentTime > MOVING_DURATION) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    hasGateOpened = false;
    gateClosing = false;
    
    if (IS_LIGHT_ENABLE) {
      digitalWrite(LIGHT_PIN, LOW); // Turn off the lights now that the gates are closed
    }
  }
}

void setup(){
  if (IS_DEBUG) {
    Serial.begin(9600); // open the serial port at 9600 bps:
    while (!Serial) {
      return; // wait for serial port to connect. Required for debugging.
    }
  }

  Utils::init(IS_DEBUG);
  Utils::test();
  
  if (IS_DEBUG) {
    Serial.println();
    Serial.println("DEBUG MODE");
    Serial.println();
  }

  while (WiFi.status() != WL_CONNECTED) {
    connectWifi(200);
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

  Serial.read();

  if (IS_DEBUG) {
    Serial.print("TRIGGERING: ");
    Serial.println(openTrigger == LOW ? "Yes" : "No");
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

  if (gateOpening) {
    openGate();
    return;
  }

  if (gateClosing) {
    closeGate();
    return;
  }
}
