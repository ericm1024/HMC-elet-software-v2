#include "elet.h"

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

  Serial.print("pressure, ");
  Serial.print(r1.digital);
  Serial.print(", ");
  Serial.print(r1.analog);
  Serial.print(", ");
  Serial.print(r2.digital);
  Serial.print(", ");
  Serial.println(r2.analog);

  delay(500);
}
