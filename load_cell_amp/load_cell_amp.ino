#include "elet_arduino.h"

char buf[64];
int i = 0;
float mass = 0;
long count = 0;

void setup() {
  Serial.begin(9600);
  memset(buf, 0, sizeof buf);
}

void loop() {
  Serial.print(count++);
  Serial.print(", ");
  Serial.print(mass, 3);
  Serial.print(", ");
  Serial.println(read_load_cell());
  delay(1000);

  int c;
  while ((c = Serial.read()) != -1) {
    if (c == '\r') {
      mass = atof(buf);
      i = 0;
      memset(buf, 0, sizeof buf);
    } else if (i == (sizeof buf) - 1) {
      Serial.println("buffer overflow. resetting to beginning");
      i = 0;
      memset(buf, 0, sizeof buf);
    } else {
      buf[i++] = c;
    }
  }
}
