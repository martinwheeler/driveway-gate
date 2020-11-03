/**
 * This project controls two DC motors to open and close a driveway gate. It also controls some lights which will be turned on during operation of the gate.
 */
 
#include "Utils.h"

const String VERSION = "1.0.1";
const int IS_LIGHT_ENABLE = 1;

// The pins below are the GPIO numbers found on the back of the Wemos D1 R2 board. Which is why the digital pin number is also mentioned in comments.
const int RIGHT_MOTOR = 2;           // Cable colour: BLUE, Digital Pin: D9
const int LEFT_MOTOR = 13;         // Cable colour: WHITE, Digital Pin: D7
const int GATE_DIRECTION = 14;      // Cable colour: BROWN, Digital Pin: D5
const int TRIGGER_PIN = 5;          // Cable colour: BLACK, Digital Pin: D3
const int LIGHT_PIN = 4;            // Cable colour: ORANGE, Digital Pin: D4

int openTrigger = LOW;
bool hasGateOpened = false, gateOpening = false, gateClosing = false, lightsOn = false;

const int OPEN_DURATION = 72000; // 72 seconds ~ 72 - 12 ~ 60 seconds total
const int LEFT_CLOSE_DELAY = 5500; // 5.5 seconds
const int RIGHT_OPEN_DELAY = 350; // 0.35 seconds
const int MOVING_DURATION = 15000; // 15 seconds
const int LIGHT_ON_DURATION = 132000; // 102 seconds ~ 132 - 72 ~ 60 seconds total

unsigned long currentTime = 0, gateOpeningTime = 0, gateClosingTime = 0, currentLightTime = 0, lightOnTime = 0;

void openGate() {
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
  }
}

void setup(){  
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

void handleLights () {  
  if (IS_LIGHT_ENABLE) {
    if (gateOpening && currentTime < 100 && !lightsOn) {
      digitalWrite(LIGHT_PIN, HIGH); // Turn on the lights now that the gates are opening
      lightOnTime = millis();
      lightsOn = true;
    }

    if (lightsOn) {
      currentLightTime = millis() - lightOnTime;

      // NOTE: We don't worry about the gate closing as a trigger because we occasionally leave the gates open
      if (currentLightTime > LIGHT_ON_DURATION) {
        digitalWrite(LIGHT_PIN, LOW); // Turn off the lights now that the time has passed
        lightsOn = false;
      }
    }
  }
}
  
void loop(){
  openTrigger = digitalRead(TRIGGER_PIN);

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
