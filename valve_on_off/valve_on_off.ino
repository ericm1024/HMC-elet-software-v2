
enum valve dut;

void setup() {
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(22, 0);
  delay(5000);
  digitalWrite(22, 1);
  delay(5000);
}
