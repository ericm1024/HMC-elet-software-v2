#include "Adafruit_MAX31855.h"

#define MAXDO   48
#define MAXCS   50
#define MAXCLK  52

Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

void setup() {  
  Serial.begin(9600);
  
  Serial.println("MAX31855 test");
  // wait for MAX chip to stabilize
  delay(500);
}

void loop() {
   double c = thermocouple.readFarenheit();
   if (isnan(c)) {
     Serial.println("Something wrong with thermocouple!");
   }
   Serial.println(c);
 
   delay(1000);
}
