int switchPin = 2;
int counter = 0;
bool lastState = HIGH;  // not pressed (INPUT_PULLUP idles HIGH)

void setup() {
  pinMode(switchPin, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  bool state = digitalRead(switchPin);

  // Detect transition from not-pressed (HIGH) to pressed (LOW)
  if (lastState == HIGH && state == LOW) {
    counter++;
    Serial.print("Count: ");
    Serial.println(counter);
    delay(50);  // debounce
  }

  lastState = state;
}