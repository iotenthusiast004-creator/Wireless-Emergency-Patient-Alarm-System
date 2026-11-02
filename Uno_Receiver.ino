// Uno: stricter EV1527 detector. Receiver DATA -> D2
const byte RX_PIN = 2;
const unsigned long T_MIN = 15, T_MAX = 600;   // plausible unit time in us (covers 35 us and 350 us)

volatile unsigned long lastEdge = 0, highW = 0, unitT = 0;
volatile unsigned long edgeCount = 0, highTime = 0, syncCount = 0, candCount = 0, frameCount = 0;
volatile int8_t nbits = -1;
volatile uint32_t frame = 0, prevFrame = 0, result = 0;
volatile bool ready = false;

void isr() {
  unsigned long now = micros();
  unsigned long d = now - lastEdge;
  lastEdge = now;
  edgeCount++;

  if (!(PIND & _BV(PD2))) {            // falling edge: d = high width
    highW = d; highTime += d;
    return;
  }
  // rising edge: d = low width
  if (highW < T_MIN) { nbits = -1; return; }

  // sync = 1T high + 31T low
  if (highW <= T_MAX && d >= 24UL * highW && d <= 40UL * highW) {
    syncCount++; unitT = highW; nbits = 0; frame = 0;
    return;
  }
  if (nbits < 0) return;

  // each bit must total about 4T and be clearly a 1 (3:1) or a 0 (1:3)
  unsigned long tot = highW + d;
  if (tot < 3 * unitT || tot > 5 * unitT) { nbits = -1; return; }
  if (highW > 2 * d)      frame = (frame << 1) | 1;
  else if (d > 2 * highW) frame = (frame << 1);
  else { nbits = -1; return; }

  if (++nbits == 24) {
    candCount++;                                   // structurally valid frame
    if (frame == prevFrame) { frameCount++; result = frame; ready = true; }
    prevFrame = frame;
    nbits = -1;
  }
}

unsigned long lastReport = 0;

void setup() {
  Serial.begin(115200);
  pinMode(RX_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RX_PIN), isr, CHANGE);
  Serial.println(F("Ready. Compare readings with the ESP32 OFF, then with the button held."));
  lastReport = millis();
}

void loop() {
  if (ready) {
    noInterrupts(); uint32_t r = result; ready = false; interrupts();
    Serial.print(F(">>> FRAME  address=0x")); Serial.print(r >> 4, HEX);
    Serial.print(F("  key="));                Serial.println(r & 0xF, BIN);
  }

  unsigned long now = millis();
  if (now - lastReport < 1000) return;
  unsigned long dt = now - lastReport; lastReport = now;

  noInterrupts();
  unsigned long edges = edgeCount, hi = highTime, syncs = syncCount, cand = candCount, frames = frameCount;
  edgeCount = highTime = syncCount = candCount = frameCount = 0;
  interrupts();

  Serial.print(F("edges/s=")); Serial.print(edges * 1000UL / dt);
  Serial.print(F(" high%="));  Serial.print(hi / (dt * 10UL));
  Serial.print(F(" syncs="));  Serial.print(syncs);
  Serial.print(F(" valid="));  Serial.print(cand);
  Serial.print(F(" repeated=")); Serial.print(frames);
  Serial.print(F(" -> "));

  if (edges == 0)       Serial.println(F("NO EDGES - check receiver wiring"));
  else if (frames > 0)  Serial.println(F("E160 FRAMES DECODED"));
  else if (cand > 0)    Serial.println(F("valid frames seen, not repeating - very close"));
  else if (syncs > 0)   Serial.println(F("sync-like pulses only (probably noise)"));
  else                  Serial.println(F("idle noise"));
}