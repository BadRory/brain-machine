# Brain Machine — Stage 1 Hardware & Wiring

A modern, open-source fork of Mitch Altman's Sound & Light Machine.
Stage 1 = ESP32-S3 core + real stereo audio + PWM glasses LEDs + ear PPG
heart rate. Designed so a future EEG front-end (Stage 3) drops in without
a redesign.

Credit: forked from Mitch Altman's Brain Machine (CC BY-SA). Keep attribution.

---

## 1. Design goals

1. Do everything the original does, better: many sessions, smooth LED fades,
   real audio (binaural beats + soundscapes), not just square-wave beeps.
2. Read heart rate from the **ear** (earlobe / behind-ear PPG).
3. Leave the door open for EEG: a free SPI bus, spare GPIOs, a clean analog
   ground, and a battery-only power path.
4. Be safe to wear on the head: hard brightness/frequency caps, a physical
   stop button, battery-only.

---

## 2. Bill of materials (Stage 1)

| Ref | Part | Role | Interface | Notes |
|-----|------|------|-----------|-------|
| U1 | ESP32-S3 DevKit (N8R2 or N16R8) | MCU / brain | — | Dual I2S + free SPI + BLE/WiFi |
| U2 | PCM5102A I2S DAC breakout | audio out | I2S0 | Line/headphone level, 3.5 mm jack onboard |
| U3 | MAX30102 breakout | ear heart rate (PPG) | I2C | Earlobe clip or behind-ear pad |
| D1,D2 | 5 mm diffused LEDs (warm white or red) | glasses light | PWM (LEDC) | One per eye, independent channels |
| Q1,Q2 | 2N7002 / AO3400 N-MOSFET (opt.) | LED drive | — | Only if LEDs exceed GPIO current |
| J1 | 3.5 mm stereo jack | headphones | — | On PCM5102 board |
| SW1 | Momentary push button | start / stop / select | GPIO | With debounce |
| ENC1 | Rotary encoder (optional) | session select | 2x GPIO | Replaces multi-click UI |
| BAT1 | 1-cell LiPo 3.7 V, 1000–2000 mAh | power | — | Battery-only during use |
| U4 | TP4056 + DW01 (or MCP73831) | charge / protect | — | Charge only when NOT worn |
| U5 | MT3608 boost or 3.3 V LDO | regulation | — | See power section |
| — | Slide switch | hard power cut | — | True off |

Resistor/cap support: LED series resistors, button pull-up, I2C pull-ups
(usually on breakout), decoupling caps.

---

## 3. System block diagram

```
                 +-----------------------------+
   LiPo 3.7V --->| TP4056 charge + protection  |
                 +--------------+--------------+
                                | (battery-only when worn)
                                v
                 +-----------------------------+
                 | 3.3V regulator (LDO/buck)   |
                 +--------------+--------------+
                                | 3V3
          +---------------------+-----------------------+
          |                     |                       |
          v                     v                       v
   +-------------+      +----------------+       +---------------+
   | ESP32-S3    | I2S  | PCM5102A DAC   |  ===> | 3.5mm jack    | -> headphones
   | (U1)        |----->| (U2)           |       +---------------+
   |             |
   |        LEDC |---[R]--> D1  (left eye LED)
   |        LEDC |---[R]--> D2  (right eye LED)
   |             |
   |         I2C |<-------> MAX30102 (U3)  ear PPG  (SDA/SCL)
   |             |
   |        GPIO |<-------- SW1 start/stop button
   |        GPIO |<-------> ENC1 encoder (optional)
   |             |
   |   SPI (FREE)|.......... reserved for Stage 3 EEG (ADS1299/ESP-EEG)
   |  GPIO(FREE) |.......... reserved: DRDY, CS, reset for EEG
   +-------------+
```

---

## 4. Pin map (ESP32-S3)

These are safe, commonly free S3 pins. Adjust to your exact devkit silkscreen;
avoid strapping pins (0, 3, 45, 46) and the USB pins (19/20) for I/O.

| Signal | GPIO | Bus | Direction | Note |
|--------|------|-----|-----------|------|
| I2S BCLK | 15 | I2S0 | out | to PCM5102 BCK |
| I2S LRCLK (WS) | 16 | I2S0 | out | to PCM5102 LRCK |
| I2S DOUT | 17 | I2S0 | out | to PCM5102 DIN |
| I2C SDA | 8 | I2C0 | bidir | MAX30102 SDA (+4.7k pull-up) |
| I2C SCL | 9 | I2C0 | out | MAX30102 SCL (+4.7k pull-up) |
| PPG INT | 10 | — | in | MAX30102 interrupt (optional) |
| LED left | 4 | LEDC | out (PWM) | series R; MOSFET if high current |
| LED right | 5 | LEDC | out (PWM) | series R |
| Button | 6 | — | in | to GND, internal pull-up, debounce |
| Encoder A | 7 | — | in | optional |
| Encoder B | 18 | — | in | optional |
| **EEG CS** | 11 | SPI | out | **reserved Stage 3** |
| **EEG SCLK** | 12 | SPI | out | **reserved Stage 3** |
| **EEG MOSI** | 13 | SPI | out | **reserved Stage 3** |
| **EEG MISO** | 14 | SPI | in | **reserved Stage 3** |
| **EEG DRDY** | 21 | — | in | **reserved Stage 3** |

Rule: do not assign the reserved block to anything in Stage 1. That is the
promise that keeps EEG from forcing a redesign.

---

## 5. Audio: PCM5102A (U2)

| PCM5102 pin | Connect to | Note |
|-------------|-----------|------|
| VIN | 3V3 | |
| GND | GND | |
| BCK | GPIO15 | I2S bit clock |
| LRCK (LCK) | GPIO16 | word select |
| DIN | GPIO17 | I2S data |
| SCK | GND | tie low: use internal PLL (no MCLK) |
| FMT | GND | I2S format |
| XMT | 3V3 | un-mute (soft-mute off) |
| 3.5 mm jack | — | to headphones |

Why this over the original beeper: the DAC lets you play binaural beats
(e.g. 200 Hz left, 210 Hz right -> 10 Hz alpha beat) and layer soft noise or
pads, so sessions sound pleasant, not harsh. Light and audio pulse off the
same master clock in firmware so they stay phase-locked.

---

## 6. Ear heart rate: MAX30102 (U3)

| MAX30102 pin | Connect to |
|--------------|-----------|
| VIN / VDD | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |
| INT | GPIO10 (optional, for data-ready IRQ) |

Placement: earlobe clip (best SNR) or a pad behind the ear built into the
headband arm. The user sits still during a session, so motion artefact — the
usual weakness of ear PPG — is minimal. In-ear PPG is validated against ECG in
the literature; earlobe is the easiest reliable spot.

Firmware reads the raw PPG, finds beats, and derives BPM + inter-beat interval
(for HRV later). Stage 2 uses this to steer the session: as HR slows and HRV
rises, ease deeper.

If behind-ear SNR is poor with the red/IR MAX30102, swap to a green-LED
reflective PPG (e.g. MAX30101) — same I2C wiring, drop-in.

---

## 7. Glasses LEDs (D1, D2)

- One diffused LED per eye, mounted in opaque safety glasses so the wearer
  sees the glow with eyes closed.
- Drive from LEDC PWM for smooth fades and per-eye control (enables alternating
  L/R patterns, not just both-on/both-off).
- Series resistor sized for your LED. If forward current exceeds ~20 mA or you
  want brighter, drive through a small N-MOSFET (Q1/Q2) from 3V3 instead of
  straight off the GPIO.

Safety cap (enforced in firmware, not just hardware):
- Max flash frequency clamp (keep the dangerous 15–25 Hz photosensitive band
  gated behind an explicit opt-in; default sessions avoid sustained dwell there).
- Max brightness / duty cap.
- Soft-start ramp, never an instant full-brightness strobe.

---

## 8. Controls

- **SW1** push button: short press = start/stop; multi-click or hold = select
  session. Wire to GND, use the ESP32 internal pull-up, debounce in firmware.
- **ENC1** rotary encoder (optional but nicer): scroll sessions, click to run.
- Config/design of sessions also over BLE/WiFi from a phone (Stage 1 firmware).

---

## 9. Power — and why it matters for EEG later

- Run from the **LiPo only** while the device is on a head. Never run sessions
  while plugged into a charger/USB: mains-referenced noise and (worst case) a
  fault path are exactly what you must avoid once electrodes touch skin.
- Charge path (TP4056) is physically separate; a slide switch gives a true off.
- Keep a single-point (star) ground. On the future board revision, give the
  analog/EEG section its own quiet ground pour tied to digital ground at one
  point. EEG signals are microvolts — layout noise discipline decided now is
  far cheaper than a respin later.

Regulator choice:
- If your ESP32-S3 devkit already has an onboard 3.3 V regulator fed from a
  battery pin, you may not need U5 for breadboarding.
- For a custom board, a low-noise LDO on the analog rail (feeding a future EEG
  front-end) beats a switching buck for signal quality.

---

## 10. Build order (de-risk)

1. ESP32-S3 + PCM5102 only. Get a binaural-beat tone out the jack.
2. Add D1/D2 PWM LEDs. Phase-lock light to the audio beat.
3. Add SW1 + session select. Run a full down-to-theta-and-back sequence.
4. Add MAX30102 on the ear. Display live BPM (serial/BLE) — read only, no
   control yet.
5. (Stage 2) Close the loop: HR/HRV nudges the session.
6. (Stage 3) Populate the reserved SPI block with an EEG front-end.

---

## 11. EEG-compatibility checklist (the "don't box us out" promise)

- [x] MCU is ESP32-S3 (has spare I2S + SPI + GPIO, BLE).
- [x] SPI bus + CS/SCLK/MOSI/MISO/DRDY GPIOs reserved and unused in Stage 1.
- [x] I2C (PPG) and SPI (EEG) are separate buses — no contention.
- [x] Battery-only operation path; charging isolated from wear.
- [x] Star ground now; dedicated analog ground pour planned for custom PCB.
- [x] Headband mechanics leave room near forehead/behind-ear for future
      electrode pads (same ear zone the PPG uses).
