#include "elet_arduino.h"

void setup() {  
  Serial.begin(9600);

  thermocouples[TC_OXYGEN].begin();

  delay(500);
  
  thermocouples[TC_WATER].begin();

  // wait for chip to chill
  delay(500);
}

void loop() {
  float oxT = read_thermocouple_f(TC_OXYGEN);
  delay(10);
  float waterT = read_thermocouple_f(TC_WATER);

  Serial.print("ox=");
  Serial.print(oxT);
  Serial.print(", water=");
  Serial.println(waterT);
  delay(1000);
}
