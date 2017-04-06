#include "elet.h"

void setup() {
  Serial.begin(9600);
  setup_igniter();
}

void loop() {
  Serial.println("about to test fire, press any key to continue");
  Serial.setTimeout(1000UL * 60UL * 60UL);
  while (Serial.read() == -1)
    ;

  enum ignition_status stat = igniter_test_fire();
  Serial.print("Ignition status was ");
  Serial.println(ignition_status_to_str(stat));
}
