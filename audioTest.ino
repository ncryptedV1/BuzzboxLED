void setup() {
  Serial.begin(9600);
}

void loop() {
  long curSig = analogRead(13);
  Serial.println(curSig);
}
