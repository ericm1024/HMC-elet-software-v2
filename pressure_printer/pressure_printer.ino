#include "elet_arduino.h"

void setup() {
  Serial.begin(9600);

  Serial.println("######################################");
  Serial.println("####### pressure sensor reader #######");
  Serial.println("######################################");
  Serial.println("");

  Serial.println("test name, digital P (yellow), analog P (yellow), digital P (blue), analog P (blue)");

}

void loop() {
  // yellow
  struct pressure_reading r1 = read_pressure(PS_OXYGEN);

  // blue
  struct pressure_reading r2 = read_pressure(PS_FUEL);

  Serial.print("ox=");
  Serial.print(r1.analog);
  Serial.print(", fuel=");
  Serial.println(r2.analog);

  delay(500);
}
