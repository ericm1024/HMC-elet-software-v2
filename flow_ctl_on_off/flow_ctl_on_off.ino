void setup() {
  // put your setup code here, to run once:
  pinMode(11, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  analogWrite(11, 0);
  Serial.println("valve is closed");
  delay(4000);
  analogWrite(11, 255);
  Serial.println("valve is open");
  delay(4000);
}
