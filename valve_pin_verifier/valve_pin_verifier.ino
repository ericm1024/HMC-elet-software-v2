#include "elet_arduino.h"

void setup() {
  Serial.begin(9600);

  // look all pretty n shit
  Serial.println("***********************************************");
  Serial.println("********** valve pin validation test **********");
  Serial.println("***********************************************");
  Serial.println("");
  
  for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v)) {
    int pin = valve_pin(v);

    // "Validating oxygen on/off valve: the valve will be controlled by pin 0"
    Serial.print("Validating ");
    Serial.print(valve_name(v));
    Serial.print(" valve: the valve will be controlled by pin ");
    Serial.print(pin);
    Serial.println(".");

    // toggle the pin
    Serial.println("Toggling pin, is the valve making noise/moving? (y/n)");
    pinMode(pin, OUTPUT);
    for (;;) {
      if (valve_is_flow(v)) {
        analogWrite(pin, 255);
        delay(500);
        analogWrite(pin, 0);
        delay(500);
      } else {
        digitalWrite(pin, 1);
        delay(500);
        digitalWrite(pin, 0);
        delay(500);
      }

      int data = Serial.peek();
      if (data == -1)
        // no response from the user yet
        continue;
       
      bool done = false;
      // okay, the user says the valve is moving, so we're good to go onto the next valve
      if (data == 'y')
        done = true;

      // otherwise, if the user didn't type 'n', be bitchy
      else if (data != 'n')
        Serial.println("please enter 'y' or 'n'");

      // clear any other bullshit the user may have typed
      while (Serial.read() != -1)
        ;

      // go onto the next valve
      if (done)
        break;
    }
  }
  // we're done
  Serial.println("");
  Serial.println("Finished testing all valves");
  Serial.end();
}

void loop() {
  // put your main code here, to run repeatedly:
}
