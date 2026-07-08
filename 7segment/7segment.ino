#include <avr/sleep.h>
#include <avr/power.h>

const int data = 12;
const int latch = 11;
const int CLK = 9;
const int leftSeg = 5;
const int rightSeg = 4;
int score = 0;

int digits[10][8] {
  {0,1,1,1,1,1,1,0}, //digit 0
  {0,0,1,1,0,0,0,0}, //digit 1
  {0,1,1,0,1,1,0,1}, //digit 2
  {0,1,1,1,1,0,0,1}, //digit 3
  {0,0,1,1,0,0,1,1}, //digit 4
  {0,1,0,1,1,0,1,1}, //digit 5
  {0,1,0,1,1,1,1,1}, //digit 6
  {0,1,1,1,0,0,0,0}, //digit 7
  {0,1,1,1,1,1,1,1}, //digit 8
  {0,1,1,1,1,0,1,1}  //digit 9
};

const int SWITCH_PIN = 2;
volatile bool awake = false;

void switchISR() {
  // read the actual pin state instead of assuming direction
  awake = (digitalRead(SWITCH_PIN) == LOW);
}

void goToSleep() {
  digitalWrite(leftSeg, LOW);
  digitalWrite(rightSeg, LOW);

  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
  delay(50);
}

void shiftOutPattern(int d) {
  digitalWrite(latch, LOW);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CLK, LOW);
    digitalWrite(data, digits[d][i] == 1 ? HIGH : LOW);
    digitalWrite(CLK, HIGH);
  }
  digitalWrite(latch, HIGH);
}

void DisplayDigit(int number) {
  digitalWrite(leftSeg, LOW);
  digitalWrite(rightSeg, LOW);
  if (number < 10) {
    shiftOutPattern(number);
    digitalWrite(rightSeg, HIGH);
    delay(5);
    digitalWrite(rightSeg, LOW);
  } else {
    int tens = number / 10;
    int ones = number % 10;
    shiftOutPattern(tens);
    digitalWrite(leftSeg, HIGH);
    delay(5);
    digitalWrite(leftSeg, LOW);
    shiftOutPattern(ones);
    digitalWrite(rightSeg, HIGH);
    delay(5);
    digitalWrite(rightSeg, LOW);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(data, OUTPUT);
  pinMode(latch, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(leftSeg, OUTPUT);
  pinMode(rightSeg, OUTPUT);
  pinMode(proxSensor, INPUT);

  // single interrupt watches both directions forever, never detached
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, CHANGE);

  // set initial state from actual switch position at boot
  awake = (digitalRead(SWITCH_PIN) == LOW);
}

void loop() {
  if (!awake) {
    goToSleep();
  }

  if(digitalRead(proxSensor) == LOW) {
    score++;
    unsigned long start = millis();
    while (millis() - start < 300) {
      if (!awake) return;
      DisplayDigit(score);
    }
 
  }
}