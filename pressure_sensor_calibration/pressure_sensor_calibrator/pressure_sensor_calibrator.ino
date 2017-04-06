
void setup() {
  Serial.begin(9600);
}

static float digital_to_psi(int digital)
{
  return ((float)(digital - 205))*1.221;
}

void loop() {
  int pressure = analogRead(0);
  Serial.print(pressure);
  Serial.print(", ");
  Serial.println(digital_to_psi(pressure));
  delay(1000);
}

