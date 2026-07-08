#include <Servo.h>
#include <avr/sleep.h>

// ---------- 7-seg / shift register ----------
const int data = 12;
const int latch = 11;
const int CLK = 9;
const int leftSeg = 5;
const int rightSeg = 4;

// ---------- scoring ----------
const int SCORE_SWITCH_PIN = 2;
const uint8_t PIEZO_PIN = A1;
const int PIEZO_THRESHOLD = 50;              // tune: watch "Piezo:" prints
const unsigned long PIEZO_LOCKOUT_MS = 250;  // ring-down time, one hit = one point

// ---------- speaker ----------
const int SPEAKER_PIN = 22;
const unsigned int TONE_SWITCH = 880;  // switch score sound (Hz)
const unsigned int TONE_PIEZO = 1320;  // piezo score sound (Hz)
const unsigned long TONE_MS = 80;

// ---------- L298N motor ----------
const int ENA = 8;   // was 6 (broken); pin 8 = Timer4 PWM, clear of tone()/Servo
const int IN1 = 7;   // unchanged (digital)
const int IN2 = 24;  // moved off pin 8 to a free digital pin
const int MIN_SPEED = 120;
const int MAX_SPEED = 255;

// ---------- IR beam break (auto-cal, scaled to small signals) ----------
const uint8_t IR_EMITTER_PIN = 13;
const uint8_t IR_RECEIVER_PIN = A0;
long baseline = 0;
int breakMargin = 6;  // recomputed from baseline at calibration
const int HYST = 2;
const unsigned long REARM_MS = 60;
const unsigned long CONFIRM_MS = 2;
const int STARTING_LIVES = 3;

// ---------- servo gate ----------
const int SERVO_PIN = 3;
const int GATE_CLOSED = 0;
const int GATE_OPEN = 90;
Servo gate;

// ---------- power switch (interrupt + sleep) ----------
const int POWER_SWITCH_PIN = 19;  // Mega interrupt pin (2 & 3 are in use)
volatile bool awake = false;

// ---------- state ----------
int score = 0;
int lives = STARTING_LIVES;
bool beamClear = true, rawBeamClear = true;
unsigned long rawChangedAt = 0;
bool armed = true;
unsigned long beamClearSince = 0;

bool lastSwState = HIGH;
unsigned long lastScoreTime = 0;
const unsigned long DEBOUNCE_MS = 50;
unsigned long lastPiezoHit = 0;

unsigned long lastRawPrint = 0;
const unsigned long RAW_PRINT_MS = 150;

int digits[10][8]{
  { 0, 1, 1, 1, 1, 1, 1, 0 }, { 0, 0, 1, 1, 0, 0, 0, 0 }, { 0, 1, 1, 0, 1, 1, 0, 1 }, { 0, 1, 1, 1, 1, 0, 0, 1 }, { 0, 0, 1, 1, 0, 0, 1, 1 }, { 0, 1, 0, 1, 1, 0, 1, 1 }, { 0, 1, 0, 1, 1, 1, 1, 1 }, { 0, 1, 1, 1, 0, 0, 0, 0 }, { 0, 1, 1, 1, 1, 1, 1, 1 }, { 0, 1, 1, 1, 1, 0, 1, 1 }
};

void powerISR() {
  awake = (digitalRead(POWER_SWITCH_PIN) == LOW);
}

void moveGate(int angle, const __FlashStringHelper* label) {
  gate.write(angle);
  Serial.print(F("SERVO COMMAND -> "));
  Serial.print(label);
  Serial.print(F(" ("));
  Serial.print(angle);
  Serial.println(F(" deg)"));
}

int scoreToSpeed(int s) {
  int level = s / 10;
  if (level > 9) level = 9;
  if (level == 0) return 0;
  return map(level, 1, 9, MIN_SPEED, MAX_SPEED);
}

void applyMotor() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, scoreToSpeed(score));
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}

void addPoint(unsigned int toneHz, const __FlashStringHelper* src) {
  score++;
  applyMotor();
  tone(SPEAKER_PIN, toneHz, TONE_MS);  // non-blocking with a duration
  Serial.print(src);
  Serial.print(F(" -> Score: "));
  Serial.println(score);
}

void calibrateBeam() {
  Serial.println(F("Calibrating beam... keep it CLEAR."));
  long sum = 0;
  for (int i = 0; i < 64; i++) {
    sum += analogRead(IR_RECEIVER_PIN);
    delay(2);
  }
  baseline = sum / 64;
  breakMargin = max(6, (int)(baseline / 2));  // scales to weak signals (e.g. baseline 20 -> margin 10)
  Serial.print(F("Baseline = "));
  Serial.print(baseline);
  Serial.print(F("  breakMargin = "));
  Serial.println(breakMargin);
}

void newGame() {
  score = 0;
  lives = STARTING_LIVES;
  beamClear = true;
  rawBeamClear = true;
  armed = true;
  beamClearSince = millis();
  rawChangedAt = millis();
  moveGate(GATE_CLOSED, F("CLOSE"));
  applyMotor();  // score 0 -> motor off
  Serial.println(F("=== NEW GAME ==="));
  Serial.print(F("Lives: "));
  Serial.println(lives);
}

void goToSleep() {
  // turning OFF resets the current game
  stopMotor();
  noTone(SPEAKER_PIN);
  moveGate(GATE_CLOSED, F("CLOSE"));
  digitalWrite(leftSeg, LOW);
  digitalWrite(rightSeg, LOW);
  Serial.println(F("Game reset. Sleeping..."));
  Serial.flush();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_cpu();  // wakes on the pin-18 interrupt
  sleep_disable();
  delay(50);
  if (awake) newGame();  // turning ON starts a fresh game
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

unsigned long lastDigitSwap = 0;
const unsigned long DIGIT_MS = 5;
bool showTens = false;

void refreshDisplay(int number) {
  if (number > 99) number = 99;
  if (millis() - lastDigitSwap < DIGIT_MS) return;
  lastDigitSwap = millis();
  digitalWrite(leftSeg, LOW);
  digitalWrite(rightSeg, LOW);
  if (number < 10) {
    shiftOutPattern(number);
    digitalWrite(rightSeg, HIGH);
  } else {
    showTens = !showTens;
    if (showTens) {
      shiftOutPattern(number / 10);
      digitalWrite(leftSeg, HIGH);
    } else {
      shiftOutPattern(number % 10);
      digitalWrite(rightSeg, HIGH);
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SCORE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(POWER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(data, OUTPUT);
  pinMode(latch, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(leftSeg, OUTPUT);
  pinMode(rightSeg, OUTPUT);
  pinMode(IR_EMITTER_PIN, OUTPUT);
  digitalWrite(IR_EMITTER_PIN, HIGH);
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);

  gate.attach(SERVO_PIN);
  Serial.println(F("Servo self-test..."));
  moveGate(GATE_OPEN, F("OPEN"));
  delay(600);
  moveGate(GATE_CLOSED, F("CLOSE"));
  delay(600);

  calibrateBeam();

  attachInterrupt(digitalPinToInterrupt(POWER_SWITCH_PIN), powerISR, CHANGE);
  awake = (digitalRead(POWER_SWITCH_PIN) == LOW);

  if (awake) newGame();
}

void loop() {
  if (!awake) {
    goToSleep();
    return;
  }

  // ---------- score source 1: limit switch ----------
  bool swState = digitalRead(SCORE_SWITCH_PIN);
  if (lastSwState == HIGH && swState == LOW && millis() - lastScoreTime > DEBOUNCE_MS) {
    lastScoreTime = millis();
    addPoint(TONE_SWITCH, F("Switch"));
  }
  lastSwState = swState;

  // ---------- score source 2: piezo ----------
  int piezoVal = analogRead(PIEZO_PIN);
  if (piezoVal > PIEZO_THRESHOLD && millis() - lastPiezoHit > PIEZO_LOCKOUT_MS) {
    lastPiezoHit = millis();
    addPoint(TONE_PIEZO, F("Piezo"));
  }

  // ---------- IR beam break ----------
  int reading = analogRead(IR_RECEIVER_PIN);
  int deviation = abs(reading - (int)baseline);

  if (millis() - lastRawPrint >= RAW_PRINT_MS) {
    lastRawPrint = millis();
    Serial.print(F("Receiver: "));
    Serial.print(reading);
    Serial.print(F("  dev: "));
    Serial.print(deviation);
    Serial.print(F("  Piezo: "));
    Serial.print(piezoVal);
    Serial.print(F("  beam: "));
    Serial.println(beamClear ? F("CLEAR") : F("BROKEN"));
  }

  bool newRaw = rawBeamClear;
  if (deviation > breakMargin + HYST) newRaw = false;
  else if (deviation < breakMargin - HYST) newRaw = true;
  if (newRaw != rawBeamClear) {
    rawBeamClear = newRaw;
    rawChangedAt = millis();
  }

  bool prevClear = beamClear;
  if (beamClear != rawBeamClear && millis() - rawChangedAt >= CONFIRM_MS) {
    beamClear = rawBeamClear;
  }

  if (beamClear && !prevClear) {
    beamClearSince = millis();
    moveGate(GATE_CLOSED, F("CLOSE"));  // ball gone -> shut gate
  }

  if (beamClear) {
    if (!armed && (millis() - beamClearSince >= REARM_MS)) armed = true;
  } else if (armed) {
    armed = false;
    if (lives > 0) lives--;
    moveGate(GATE_OPEN, F("OPEN"));  // release the next ball
    Serial.print(F("Beam broken! Lives: "));
    Serial.println(lives);
  }

  refreshDisplay(score);
}