/**
 * This project controls two DC motors to open and close a driveway gate. It also controls some lights which will be turned on during operation of the gate.
 */
 
#include "Utils.h"

const int IS_ESP32 = 0;
const int IS_DEBUG = 0;

const int LEFT_MOTOR = IS_ESP32 ? 18 : 2;           //  Low = off    High = on // BLUE
const int RIGHT_MOTOR = IS_ESP32 ? 19 : 13;           //  Low = off    High = on // WHITE
const int GATE_DIRECTION = IS_ESP32 ? 17 : 14;       //  Low = close    High = open // BROWN
const int TRIGGER_PIN = IS_ESP32 ? 25 : 5;           //  Low = off    High = on

int openTrigger = LOW;
bool hasGateOpened = false, gateOpening = false, gateClosing = false;

const int OPEN_DURATION = 70000; // 70 seconds
const int LEFT_CLOSE_DELAY = 4750; // 4.75 seconds
const int RIGHT_OPEN_DELAY = 350; // 0.35 seconds
const int MOVING_DURATION = 12000; // 12 seconds

unsigned long currentTime = 0, gateOpeningTime = 0, gateClosingTime = 0;

void openGate() {
  if (IS_DEBUG) {
    Serial.println("OPEN");
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

  // 
  if (currentTime > MOVING_DURATION) {
    digitalWrite(RIGHT_MOTOR , LOW);
    digitalWrite(LEFT_MOTOR , LOW);
    hasGateOpened = false;
    gateClosing = false;
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
    Serial.println("DEBUG MODE");
  }
  
  pinMode(RIGHT_MOTOR, OUTPUT);
  pinMode(LEFT_MOTOR, OUTPUT);
  pinMode(GATE_DIRECTION, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  digitalWrite(GATE_DIRECTION, LOW);
  digitalWrite(LEFT_MOTOR, LOW);
  digitalWrite(RIGHT_MOTOR, LOW);
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
