# Wireless Emergency Patient Alarm System

A simple wireless emergency alarm built around the **Ebyte E160-T3F12S2** 315 MHz transmitter. A patient presses a button at the bedside and an alarm (buzzer + LED) sounds at the caretaker's station. There is no Wi-Fi and no internet involved.

> **Disclaimer:** This is a hobby/demonstration project, not a certified medical alert device. Do not rely on it where someone's safety depends on it.

## Contents

- [How it works](#how-it-works)
- [Key codes and functions](#key-codes-and-functions)
- [Hardware](#hardware)
- [Wiring](#wiring)
- [Software setup](#software-setup)
- [Code](#code)
- [Testing and troubleshooting](#testing-and-troubleshooting)
- [Limitations and improvements](#limitations-and-improvements)
- [Repository structure](#repository-structure)
- [Links](#links)

## How it works

```
 Bedside unit                                   Caretaker's station
┌───────────────────────────────┐              ┌──────────────────────────────┐
│ Buttons -> ESP32 -> E160-T3F12S2 )))  315 MHz  (((  RX module -> Arduino Uno -> Buzzer + LED │
│        (power + key simulation) │              │         (decodes EV1527 frame)            │
└───────────────────────────────┘              └──────────────────────────────┘
```

- **The E160-T3F12S2 needs no microcontroller.** It has a built-in EV1527 encoder and a unique factory-set 20-bit address. It only needs power, an antenna, and a key pin (K0, K1 or K2) pulled to ground for at least 100 ms.
- **The ESP32 is only used to supply 3.3 V and to simulate button presses** by pulling the E160's key pins low. In a finished product you can remove it and use a coin cell and real push buttons.
- **The receiver side** is an Arduino Uno with a low-cost INVENTO 315 MHz receiver. The Uno decodes the pulse train (sync, 20-bit address, 4-bit key code) and sounds a different alarm pattern for each key.

## Key codes and functions

The E160 has three independent key inputs, K0, K1 and K2. Each can be given its own function. Pressing keys together creates extra combinations. The manual lists these six keys:

| Key | Pins pulled low | Key code (from the manual) | Example function |
| --- | --- | --- | --- |
| K0 | K0 | `0001` | Call nurse |
| K1 | K1 | `0010` | Need assistance |
| K2 | K2 | `0100` | Emergency |
| K3 | Six-key reference circuit in the manual | `1000` | Spare |
| K4 | K0 + K2 together | `0101` | Water request |
| K5 | K1 + K2 together | `0110` | Medication due |

The function names are only examples. Change them in the Uno sketch. Three inputs allow seven combinations in theory, but the manual does not list K0 + K1 (`0011`) or all three together (`0111`), so test those before relying on them.

## Hardware

**Transmitter side**

| Part | Notes |
| --- | --- |
| Ebyte E160-T3F12S2 | 315 MHz OOK/ASK transmitter, 1.8 to 3.6 V |
| ESP32 DevKit (DOIT V1, 30-pin) | Supplies 3.3 V and simulates key presses |
| 3 push buttons | One per key |
| Wire for antenna | About 22 cm |
| Breadboard and jumper wires | |

**Receiver side**

| Part | Notes |
| --- | --- |
| INVENTO 315 MHz receiver module | Low-cost super-regenerative receiver, 5 V |
| Arduino Uno | |
| Buzzer | Active buzzer, or passive (the sketch uses `tone()`) |
| LED | The Uno's built-in LED on pin 13 is used by default |
| Wire for antenna | About 23 cm |

## Wiring

### Transmitter: ESP32 to E160-T3F12S2

All connections are on the **right-hand side** of the ESP32 DevKit header (USB port at the bottom).

| E160 pin | Connect to |
| --- | --- |
| VDD (pin 5) | ESP32 3V3 |
| GND (pins 6, 7, 8) | ESP32 GND (use a breadboard ground rail) |
| K0 (pin 1) | ESP32 GPIO 16 |
| K1 (pin 2) | ESP32 GPIO 17 |
| K2 (pin 3) | ESP32 GPIO 18 |
| ANT (pin 9) | Wire antenna, about 22 cm |
| NC (pin 4) | Not connected (optional LED output, see the E160 manual) |

**Buttons** (each between the GPIO and GND, using the internal pull-ups):

| Button | ESP32 pin |
| --- | --- |
| K0 button | GPIO 4 |
| K1 button | GPIO 19 |
| K2 button | GPIO 21 |

The ESP32 is powered over USB. The right-hand side has only one GND pin, so run it to a breadboard rail and connect the E160 GND pad and the button legs to that rail. GPIO 2, 5 and 15 are avoided because they are boot-strapping pins.

### Receiver: INVENTO module to Arduino Uno

| Receiver pin | Connect to |
| --- | --- |
| VCC | Uno 5V |
| GND | Uno GND |
| DATA (either DATA pin, they are tied together) | Uno D2 |
| ANT pad | Wire antenna, about 23 cm |

| Output | Connect to |
| --- | --- |
| Buzzer (+) | Uno D8 |
| Buzzer (-) | Uno GND |
| LED | Uno built-in LED on D13 (or an external LED with a 220 ohm resistor) |

The receiver's 5 V data output is safe for the Uno's D2. Do not connect this receiver directly to an ESP32 input.

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. For the ESP32, install the **esp32 by Espressif Systems** board package through the Boards Manager, and select *DOIT ESP32 DEVKIT V1*.
3. Upload `esp32_transmitter.ino` to the ESP32.
4. Select *Arduino Uno* and upload `uno_receiver.ino` to the Uno.
5. Open the Serial Monitor on the Uno at **115200 baud**.

No external libraries are needed.

## Code

Save each sketch in its own folder with the same name as the file.

<details>
<summary><b>esp32_transmitter/esp32_transmitter.ino</b></summary>

```cpp
// ESP32: simulates presses of the E160-T3F12S2 key pins K0, K1, K2.
// Holding two buttons together pulls two key pins low, which sends a combination.

const int BTN[3] = {4, 19, 21};     // buttons for K0, K1, K2 (to GND)
const int KEY[3] = {16, 17, 18};    // E160 K0, K1, K2

void setup() {
  for (int i = 0; i < 3; i++) {
    pinMode(BTN[i], INPUT_PULLUP);
    pinMode(KEY[i], OUTPUT);
    digitalWrite(KEY[i], HIGH);      // idle = not pressed
  }
}

void loop() {
  bool any = false;
  for (int i = 0; i < 3; i++) {
    bool pressed = (digitalRead(BTN[i]) == LOW);
    digitalWrite(KEY[i], pressed ? LOW : HIGH);   // active low
    any |= pressed;
  }
  if (any) delay(150);               // keep the key low at least 100 ms (manual)
}
```

</details>

<details>
<summary><b>uno_receiver/uno_receiver.ino</b></summary>

```cpp
// Arduino Uno: decodes EV1527 frames from the E160-T3F12S2 and sounds an alarm.
// Receiver DATA -> D2, buzzer -> D8.

const byte RX_PIN = 2;
const byte BUZZER = 8;
const byte LED = LED_BUILTIN;

// 0 = learning mode (accept any address and print it).
// Paste your transmitter's address here (for example 0x5A3C1) to ignore other transmitters.
const uint32_t KNOWN_ADDRESS = 0;

const bool DEBUG = true;                       // prints edges/s every second
const unsigned long T_MIN = 15, T_MAX = 600;   // plausible unit time in us

volatile unsigned long lastEdge = 0, highW = 0, unitT = 0;
volatile unsigned long edgeCount = 0, syncCount = 0;
volatile int8_t nbits = -1;
volatile uint32_t frame = 0, prevFrame = 0, result = 0;
volatile bool ready = false;

void isr() {
  unsigned long now = micros();
  unsigned long d = now - lastEdge;
  lastEdge = now;
  edgeCount++;

  if (!(PIND & _BV(PD2))) {            // falling edge: d = high width
    highW = d;
    return;
  }
  if (highW < T_MIN) { nbits = -1; return; }   // rising edge: d = low width

  // sync = 1T high + 31T low
  if (highW <= T_MAX && d >= 24UL * highW && d <= 40UL * highW) {
    syncCount++; unitT = highW; nbits = 0; frame = 0;
    return;
  }
  if (nbits < 0) return;

  // each bit totals about 4T and is clearly a 1 (3:1) or a 0 (1:3)
  unsigned long tot = highW + d;
  if (tot < 3 * unitT || tot > 5 * unitT) { nbits = -1; return; }
  if (highW > 2 * d)      frame = (frame << 1) | 1;
  else if (d > 2 * highW) frame = (frame << 1);
  else { nbits = -1; return; }

  if (++nbits == 24) {
    if (frame == prevFrame) { result = frame; ready = true; }  // must repeat twice
    prevFrame = frame;
    nbits = -1;
  }
}

void beep(byte count, unsigned int onMs, unsigned int offMs) {
  for (byte i = 0; i < count; i++) {
    tone(BUZZER, 2000); digitalWrite(LED, HIGH); delay(onMs);
    noTone(BUZZER);     digitalWrite(LED, LOW);  delay(offMs);
  }
}

unsigned long lastReport = 0;

void setup() {
  Serial.begin(115200);
  pinMode(RX_PIN, INPUT);
  pinMode(LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(RX_PIN), isr, CHANGE);
  Serial.println(F("Emergency alarm receiver ready."));
}

void loop() {
  if (ready) {
    noInterrupts(); uint32_t r = result; ready = false; interrupts();
    uint32_t address = r >> 4;
    byte key = r & 0xF;

    Serial.print(F("Frame: address=0x")); Serial.print(address, HEX);
    Serial.print(F("  key="));            Serial.println(key, BIN);

    if (KNOWN_ADDRESS == 0 || address == KNOWN_ADDRESS) {
      switch (key) {
        case 0b0001: Serial.println(F("K0: Call nurse"));        beep(2, 300, 200); break;
        case 0b0010: Serial.println(F("K1: Need assistance"));   beep(3, 300, 200); break;
        case 0b0100: Serial.println(F("K2: EMERGENCY"));         beep(8, 120, 80);  break;
        case 0b0101: Serial.println(F("K4 (K0+K2): Water request"));  beep(4, 150, 100); break;
        case 0b0110: Serial.println(F("K5 (K1+K2): Medication due")); beep(5, 200, 150); break;
        case 0b1000: Serial.println(F("K3: Spare"));             beep(1, 500, 100); break;
        default:     Serial.println(F("Unknown key code"));     break;
      }
      ready = false;                    // ignore frames received during the alarm
    }
  }

  if (DEBUG && millis() - lastReport >= 1000) {
    unsigned long dt = millis() - lastReport; lastReport = millis();
    noInterrupts(); unsigned long e = edgeCount, s = syncCount; edgeCount = 0; syncCount = 0; interrupts();
    Serial.print(F("edges/s=")); Serial.print(e * 1000UL / dt);
    Serial.print(F("  syncs="));  Serial.println(s);
  }
}
```

</details>

## Testing and troubleshooting

1. Upload both sketches and open the Uno's Serial Monitor at 115200 baud.
2. With the transmitter off, note the `edges/s` value. This is the receiver's idle noise.
3. Press and hold a button on the ESP32 side, about 20 to 50 cm from the receiver, with both antennas fitted. You should see `Frame: address=... key=...` lines and hear the buzzer.
4. Note the address printed on a press and paste it into `KNOWN_ADDRESS` so only your transmitter triggers the alarm.
5. Press each key, and each combination, once. Check the key codes printed in the Serial Monitor. The manual's table shows `0001` for K0. If your codes appear with the bits in the opposite order, adjust the `case` values to match what you see.

| Problem | Things to try |
| --- | --- |
| `edges/s=0` | Check the receiver's VCC, GND and DATA wiring. |
| Only idle noise, no frames | Fit both antennas. Slowly turn the receiver's red coil with a plastic screwdriver while pressing a button. Try a shorter distance. |
| Still nothing | Check the receiver's frequency. Some listings include a 433 MHz receiver or transmitter with 315 MHz in the title. The E160-T3F12S2 transmits at 315 MHz. |
| Frames missed or unreliable | The cheap receiver may be too slow for the E160's 28 kbps signal. See the next section. |
| No transmission at all | Confirm the ESP32 pulls the key pin low for at least 100 ms. Check that VDD is 1.8 to 3.6 V (not 5 V). |

## Limitations and improvements

- **The cheap receiver is the weak point.** The INVENTO module is a simple, noisy receiver. The E160 sends data at 28 kbps, and the manual specifically recommends Ebyte's **E160-RxMD2** receiver and says the slower E160-RxMS1 is not suitable. Expect short range and the occasional missed press with the INVENTO module. With the RxMD2, the manual quotes up to 210 m in a clear open area (this figure was measured by the manufacturer and will be lower indoors).
- **No acknowledgement.** The link is one-way. The transmitter does not know whether the alarm was received.
- **Battery-powered bedside unit.** The E160 needs no microcontroller and idles at about 1 uA, so the ESP32 can be replaced by a coin cell and push buttons.
- **Keep the antenna clear.** Don't put the antenna inside a metal case or next to metal objects.
- **Possible extensions:** an external siren or relay, multiple transmitters (each E160 has its own address), and a display on the receiver showing which function was triggered.

## Repository structure

```
.
├── README.md
├── esp32_transmitter/
│   └── esp32_transmitter.ino
├── uno_receiver/
│   └── uno_receiver.ino
└── docs/
    ├── transmitter_connections.png
    ├── receiver_connections.png
    └── E160-TxF12S2_UserManual_EN_v1_0.pdf
```

## Links

- Demo video: [YOUR YOUTUBE VIDEO LINK]
- LinkedIn: [YOUR LINKEDIN PROFILE LINK]
- Reddit: [YOUR REDDIT PROFILE LINK]
- E160-TxF12S2 user manual: [Ebyte website](https://www.cdebyte.com)

## License

Add your preferred license (for example MIT) as a `LICENSE` file.
