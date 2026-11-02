const int BTN = 4;            // button between GPIO 4 and GND
const int KEY0 = 16;          // E160 K0
const int LED = 2;            // onboard LED, on while transmitting

const bool PULSE_MODE = true; // true: repeated presses, false: one long press
const unsigned long PRESS_MS = 150;  // K0 low time (manual: at least 100 ms)
const unsigned long GAP_MS = 50;     // K0 high time between pulses

void setup() {
  pinMode(BTN, INPUT_PULLUP);
  pinMode(KEY0, OUTPUT);
  digitalWrite(KEY0, HIGH);   // idle = not pressed
  pinMode(LED, OUTPUT);
}

void loop() {
  if (digitalRead(BTN) != LOW) return;
  delay(20);                              // debounce
  if (digitalRead(BTN) != LOW) return;

  digitalWrite(LED, HIGH);

  if (PULSE_MODE) {
    // Every pulse is a fresh "key press" for the E160, so you get
    // a new burst of frames every ~200 ms for as long as you hold the button.
    while (digitalRead(BTN) == LOW) {
      digitalWrite(KEY0, LOW);  delay(PRESS_MS);
      digitalWrite(KEY0, HIGH); delay(GAP_MS);
    }
  } else {
    // K0 stays low for the whole hold; the E160 repeats frames by itself.
    unsigned long start = millis();
    digitalWrite(KEY0, LOW);
    while (digitalRead(BTN) == LOW) delay(5);
    while (millis() - start < PRESS_MS) delay(5);  // enforce minimum hold
    digitalWrite(KEY0, HIGH);
  }

  digitalWrite(LED, LOW);
}