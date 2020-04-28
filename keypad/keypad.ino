#include <Keypad.h>
#include "config.h"

const int TRIGGER_COUNT = 5;
const int TRIGGER_DELAY = 1000;
const int DOOR_PIN = 13;
const int GREEN_LED_PIN = 9;
const int RED_LED_PIN = 10;
const int BEEP_LED_PIN = 12;
const int IS_DEBUG = 1; // Enable/disable the serial print statements

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {7, 2, 3, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 8, 4}; //connect to the column pinouts of the keypad

//Create an object of keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

const int MAXIMUM_PRESSES = 8; // The number of key presses before the stored entry is reset
char pressed[MAXIMUM_PRESSES];
int pressedIndex = 0;

String currentEntry () {
  if (IS_DEBUG) {
    Serial.println(String(pressed)); 
  }
  return String(pressed);
}

void flashRedLED () {
  digitalWrite(RED_LED_PIN, HIGH);
  delay(2500);
  digitalWrite(RED_LED_PIN, LOW);
}

void triggerGate () {
  if (IS_DEBUG) {
    Serial.println("Opening the gate");
  }

  digitalWrite(GREEN_LED_PIN, HIGH);

  for (int i = 0; i < TRIGGER_COUNT; i++) {
    digitalWrite(DOOR_PIN, LOW);
    delay(TRIGGER_DELAY);
    digitalWrite(DOOR_PIN, HIGH);
    delay(TRIGGER_DELAY);
  }
  
  digitalWrite(GREEN_LED_PIN, LOW);
}

void addPressed (char keyPressed) {
  pressed[pressedIndex] = keyPressed;
  pressedIndex++;
}

void reset () {
  digitalWrite(DOOR_PIN, HIGH);
  pressedIndex = 0;

  for(int i = 0; i < sizeof(pressed); i++) {
    pressed[i] = (char)0;
  }
}

void playTone () {
  tone(BEEP_LED_PIN, 5000);
  delay(100);
  noTone(BEEP_LED_PIN);
}

void setup(){
  if (IS_DEBUG) {
    Serial.begin(9600);
    Serial.println("Password: ");
    Serial.print(PASSWORD);
  }
  
  pinMode(DOOR_PIN, OUTPUT);
  digitalWrite(DOOR_PIN, HIGH); // Starts high then goes low
  
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
}
  
void loop(){
  char key = keypad.getKey(); // Read the key
  
  if (key){
    playTone();
    if (IS_DEBUG) {
      Serial.print("Key Pressed : ");
      Serial.println(key);
    }

    if (key == SUBMIT_KEY) {
      if (currentEntry() == PASSWORD) {
        triggerGate();
      } else {
        flashRedLED();
      }

      reset();
    } else {
      addPressed(key);
    }
  }

  if (pressedIndex >= MAXIMUM_PRESSES) {
    flashRedLED();
    reset();
  }
}
