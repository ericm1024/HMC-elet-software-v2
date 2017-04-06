#include "elet.h"

enum pressure_sensor ps = PS_FUEL;

void setup() {
  setup_all_valves();
  
  const enum valve testing_valve = FUEL_FLOW;
  
  Serial.begin(9600);

  Serial.println("###################################################");
  Serial.println("####### flow control valve calibration test #######");
  Serial.println("###################################################");
  Serial.println("");

  // ask the user for a pwm value. Set the timeout to a while so they aren't rushed
  Serial.setTimeout(1000L*60L*60L);
  int pwm_val = -1;
  for (;;) {
    Serial.print("Please pick a pwm value in [0,255]: ");
    long val = Serial.parseInt();
    if (val < 0 || val > 255) {
      Serial.print(val);
      Serial.println("is out of range");
    } else {
      pwm_val = val;
      break;
    }
  }

  Serial.println("");
  Serial.print("opening to ");
  Serial.println(pwm_val);

  const int pin = valve_pin(testing_valve);
  pinMode(pin, OUTPUT);
  analogWrite(pin, pwm_val);
  Serial.println("valve open. Pressure data follows");
  Serial.println("digital value, un-calibrated PSI");
}

static float digital_to_psi(int digital)
{
  // the pressure sensor maps [0,1000]PSI to [1,5]V and our ADC maps [1,5]V to
  // [0,1023]. This math does adjusts down for the 1V bias, then multiplies
  // by the remaining range (818.4) divided by 1000
  return 1.3135*((float)digital) - 299.76;
}

void loop() {
  int pressure = analogRead(FUEL_PRESSURE_PIN);
  Serial.print(pressure);
  Serial.print(", ");
  Serial.println(digital_to_psi(pressure));
  delay(1000);
}


