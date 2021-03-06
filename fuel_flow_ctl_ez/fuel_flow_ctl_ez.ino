#include "elet.h"

//static const int pwm_bins[] = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255};

static const int pwm_bins[] = {0, 17, 34, 51, 68, 76, 85, 93, 102, 110, 119, 127, 136, 144, 153, 161, 170, 178, 187, 195, 204, 221, 238, 255};

unsigned bin_idx = 0;
const unsigned nbins = (sizeof pwm_bins)/(sizeof pwm_bins[0]);
const enum valve v = FUEL_FLOW;

void setup() {
  setup_all_valves();

  Serial.begin(9600);

  Serial.println("##########################################################");
  Serial.println("####### flow control valve calibration test (FUEL) #######");
  Serial.println("##########################################################");
  Serial.println("");

  Serial.println("test name, pwm value, digital pressure, analog pressure");

  close_valve(v);
}

// if someone types something on serial, move to the next bin
void serialEvent() {
  bin_idx = (bin_idx + 1) % nbins;
  analogWrite(valve_pin(v), pwm_bins[bin_idx]);
  
  // clear the serial queue but see if the user wanted to purge the system on this test
  for (;;) {
    int c = Serial.read();
    if (c == -1)
      break;
    else if (c == 'q') {
      bin_idx = 0;
      analogWrite(valve_pin(v), pwm_bins[bin_idx]);
    }
  }
}


void loop() {
  struct pressure_reading r = read_pressure(PS_FUEL);
  Serial.print("fuel, ");
  Serial.print(pwm_bins[bin_idx]);
  Serial.print(", ");
  Serial.print(r.digital);
  Serial.print(", ");
  Serial.println(r.analog);
  delay(500);
}

