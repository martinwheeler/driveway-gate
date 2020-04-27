#include <Arduino.h>
#include "Utils.h"

bool Utils::isDebugging = false;

void Utils::init(bool debugMode) {
    // TODO: Add constructor logic here
    Utils::isDebugging = debugMode;
}

void Utils::test() {
  if (Utils::isDebugging) {
    Serial.println("Door util, calling test function."); 
  }
}
