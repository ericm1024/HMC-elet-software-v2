#include <Q2HX711.h>
#define DOUT 45
#define CLK 43
Q2HX711 hx711(DOUT, CLK);
void setup() {
  Serial.begin(9600);
}

long count = 0;

uint8_t _pin_dout = DOUT;
uint8_t _pin_slk = CLK;

long getValue()
{
  byte data[3];

  while (digitalRead(_pin_dout))
    ;

  data[2] = shiftIn(_pin_dout, _pin_slk, MSBFIRST);
  data[1] = shiftIn(_pin_dout, _pin_slk, MSBFIRST);
  data[0] = shiftIn(_pin_dout, _pin_slk, MSBFIRST);

  digitalWrite(_pin_slk, HIGH);
  digitalWrite(_pin_slk, LOW);

  data[2] ^= 0x80;

  return ((uint32_t) data[2] << 16) | ((uint32_t) data[1] << 8)
      | (uint32_t) data[0];
}

void loop() {
  count = count + 1;
  getValue();
  if (count%80 == 0)
    Serial.println("read 80 samples");
  //Serial.println(getValue());
  //Serial.println(hx711.read());
}
