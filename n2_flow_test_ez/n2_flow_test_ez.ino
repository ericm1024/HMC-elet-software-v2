#include "elet.h"

const enum valve v = N2_ON_OFF;
bool is_open = false;

void setup() {
  setup_all_valves();

  Serial.begin(9600);

  Serial.println("#######################################");
  Serial.println("####### nitrogen flow test (N2) #######");
  Serial.println("#######################################");
  Serial.println("");

  Serial.println("test name, open/closed");

  close_valve(v);
}

// if someone types something on serial, change the valve's state
void serialEvent() {
  is_open = !is_open;

  if (is_open)
    open_valve(v);
  else
    close_valve(v);
  
  // clear the serial queue
  while (Serial.read() != -1)
    ;
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("nitrogen, ");
  if (is_open)
    Serial.println("open");
  else
    Serial.println("closed");
  delay(500);
}
