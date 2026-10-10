#include <Arduino.h>

const uint8_t SCL_PIN = A5;
const uint8_t SDA_PIN = A4;

const uint8_t SINE_N = 32;
uint8_t sineTable[SINE_N];
uint8_t sineIdx = 0;
uint8_t ramp = 0;

struct Step {
  uint8_t scl;
  uint8_t sda;
  uint8_t reps;
};

Step steps[90];
uint8_t nSteps = 0;
uint8_t stepIdx = 0;
uint8_t repLeft = 0;

void addStep(uint8_t scl, uint8_t sda, uint8_t reps) {
  steps[nSteps].scl = scl;
  steps[nSteps].sda = sda;
  steps[nSteps].reps = reps;
  nSteps++;
}

void addByte(uint8_t b) {
  for (int8_t i = 7; i >= 0; i--) {
    uint8_t bit = (b >> i) & 1;
    addStep(0, bit, 1);
    addStep(1, bit, 2);
  }
  addStep(0, 0, 1);
  addStep(1, 0, 2);
}

void setup() {
  Serial.begin(1000000);
  ADCSRA = (ADCSRA & 0xF8) | 0x04;
  TCCR1B = (TCCR1B & 0xF8) | 0x01;
  TCCR2B = (TCCR2B & 0xF8) | 0x01;

  pinMode(SCL_PIN, OUTPUT);
  pinMode(SDA_PIN, OUTPUT);

  analogWrite(9, 64);
  analogWrite(10, 128);

  for (uint8_t i = 0; i < SINE_N; i++) {
    sineTable[i] = (uint8_t)((sin(i * 2.0 * PI / SINE_N) + 1.0) * 127.5);
  }
  analogWrite(3, sineTable[0]);
  analogWrite(11, 0);

  addStep(1, 1, 6);
  addStep(1, 0, 2);
  addByte(0xEE);
  addByte(0x00);
  addStep(0, 0, 1);
  addStep(1, 0, 1);
  addStep(1, 1, 2);
  addStep(1, 1, 8);
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastGen = 0;
  static unsigned long lastSine = 0;
  if (now - lastGen >= 3) {
    lastGen = now;
    ramp += 3;
    analogWrite(11, ramp);
  }
  if (now - lastSine >= 12) {
    lastSine = now;
    sineIdx = (sineIdx + 1) % SINE_N;
    analogWrite(3, sineTable[sineIdx]);
  }

  if (repLeft == 0) {
    stepIdx = (stepIdx + 1) % nSteps;
    digitalWrite(SCL_PIN, steps[stepIdx].scl);
    digitalWrite(SDA_PIN, steps[stepIdx].sda);
    repLeft = steps[stepIdx].reps;
  } else {
    repLeft--;
  }

  Serial.println(analogRead(A0));
}
