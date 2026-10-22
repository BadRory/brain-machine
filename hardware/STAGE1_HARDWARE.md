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
| U1 | **Seeed XIAO ESP32-S3** (8 MB PSRAM) | MCU / brain | — | 21 × 17.8 mm, onboard LiPo charger, I2S + free SPI + I2C + WiFi/BLE |
| U2 | PCM5102A I2S DAC breakout | audio out | I2S0 | Line/headphone level, 3.5 mm jack onboard |
| U3 | MAX30102 breakout (14 × 14 mm) | ear heart rate (PPG) | I2C | Earlobe clip or behind-ear pad |
| D1,D2 | 3 mm diffused LEDs (warm white or red) | glasses light | PWM (LEDC) | One per eye, independent channels |
| Q1,Q2 | 2N7002 / AO3400 N-MOSFET (opt.) | LED drive | — | Only if LEDs exceed GPIO current |
| J1 | 3.5 mm stereo jack | headphones | — | On PCM5102 board |
| SW1 | SMD tactile push button | **stop** (blind, eyes-shut) | GPIO | The only physical control. Debounced. |
| BAT1 | 1-cell flat LiPo 3.7 V, 400–500 mAh | power | — | Small/light for head wear; to XIAO BAT pad |
| — | Slide switch | hard power cut | — | True off |

**Control is the WiFi web app, not hardware.** The XIAO hosts its own
WiFi and serves a web UI (see `CONTROL_WEBAPP.md`). That removes the rotary
encoder, the touch strip, and the multi-click button logic. The one remaining
physical part is **SW1, a single stop button** you can hit blind during a
session. No touch pads, no encoder.

**Charger:** the XIAO has LiPo charge management built in, so the standalone
**TP4056 is dropped**. It also has an onboard 3.3 V regulator fed from the BAT
pad, so no separate regulator is needed for the prototype.

Resistor/cap support: LED series resistors, button pull-up, I2C pull-ups
(usually on breakout), decoupling caps.

---

## 3. System block diagram

```
                 +-----------------------------+
   LiPo 3.7V --->| XIAO onboard charge + reg   |  (no external TP4056)
                 +--------------+--------------+
                                | 3V3 (battery-only when worn)
          +---------------------+-----------------------+
          |                     |                       |
          v                     v                       v
   +-------------+      +----------------+       +---------------+
   | XIAO        | I2S  | PCM5102A DAC   |  ===> | 3.5mm jack    | -> headphones
   | ESP32-S3    |----->| (U2)           |       +---------------+
   | (U1)        |
   |             |
   |   WiFi/BLE ((•)) <==> phone browser  -> web app (setup + history)
   |             |
   |        LEDC |---[R]--> D1  (left eye LED)
   |        LEDC |---[R]--> D2  (right eye LED)
   |             |
   |         I2C |<-------> MAX30102 (U3)  ear PPG  (SDA/SCL)
   |             |
   |        GPIO |<-------- SW1 stop button (blind, eyes-shut)
   |             |
   |   SPI (FREE)|.......... reserved for Stage 3 EEG (ADS1299/ESP-EEG)
   |  GPIO(FREE) |.......... reserved: DRDY, CS, reset for EEG
   +-------------+
```

The WiFi web app costs **zero GPIO** — it rides on the chip's radio. That is
why the design can drop the encoder/touch/multi-click button and keep only a
single stop button.

---

## 4. Pin map (XIAO ESP32-S3)

The XIAO exposes **11 GPIO**, labelled D0–D10 on the silk. Below are the
underlying ESP32-S3 GPIO numbers for each XIAO pad. Stage 1 uses **8** of 11,
leaving **3 spare**.

| Signal | XIAO pad | GPIO | Bus | Dir | Note |
|--------|----------|------|-----|-----|------|
| I2S BCLK | D1 | 2 | I2S0 | out | to PCM5102 BCK |
| I2S LRCLK (WS) | D2 | 3 | I2S0 | out | to PCM5102 LRCK |
| I2S DOUT | D3 | 4 | I2S0 | out | to PCM5102 DIN |
| I2C SDA | D4 | 5 | I2C0 | bidir | MAX30102 SDA (+4.7k pull-up) |
| I2C SCL | D5 | 6 | I2C0 | out | MAX30102 SCL (+4.7k pull-up) |
| LED left | D0 | 1 | LEDC | out (PWM) | series R; MOSFET if high current |
| LED right | D9 | 8 | LEDC | out (PWM) | series R |
| Stop button | D10 | 9 | — | in | to GND, internal pull-up, debounce |
| *(spare)* | D6 | 43 | — | — | free (also UART TX) |
| *(spare)* | D7 | 44 | — | — | free (also UART RX) |
| *(spare)* | D8 | 7 | — | — | free (also SPI pad) |

I2S/I2C pin numbers on the ESP32-S3 are flexible via the GPIO matrix, so the
exact assignment can shift; the table above is a safe, verified-against-XIAO
starting layout. PPG INT is dropped from Stage 1 (polled over I2C) to save a
pin; add it on a spare later if you want data-ready IRQ.

### EEG future on the XIAO — the honest bit

The XIAO does **not** have 5 free pins to hand a full SPI EEG front-end
(CS/SCLK/MOSI/MISO/DRDY) on top of Stage 1. The 3 spare pads (D6/D7/D8) are not
enough. This does **not** box out EEG, because:

- The XIAO is the **prototype** board. The EEG build (Stage 3) was always going
  to be a **custom PCB** with the ESP32-S3 chip placed directly, where every
  pin is broken out and the analog ground is laid out properly for microvolt
  signals. A devkit was never going to host clean EEG.
- So the promise stands at the **chip** level (ESP32-S3 has the I2S + SPI +
  GPIO + BLE for all three stages), not at the XIAO pad level. The XIAO proves
  Stages 1–2; the board spin adds Stage 3.

Rule for Stage 1: keep D8 (the SPI-capable pad) and the two spares unused where
you can, so you have room to breadboard an early EEG experiment before the
custom board exists.

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

## 8. Controls — WiFi web app + one stop button

All setup and review happen in a **web app the device hosts over WiFi**. No
encoder, no touch strip, no multi-click button logic. See
[`CONTROL_WEBAPP.md`](../firmware/CONTROL_WEBAPP.md) for the full plan.

- **Networking:** boots as its own WiFi hotspot (AP). From the web UI the user
  can hand it home WiFi credentials; it then joins that network and is reachable
  at `brainmachine.local`. If the saved network isn't found at boot, it **falls
  back to the hotspot** — the standard IoT captive-provisioning pattern.
- **During a session** your eyes are shut, so the web app is for *setup and
  review*, not live control. The one in-session control is hardware:
- **SW1** single stop button — press to end the running session. Wire to GND,
  internal pull-up, debounce in firmware. Placed where a hand finds it blind.

---

## 9. Power — and why it matters for EEG later

- Run from the **LiPo only** while the device is on a head. Never run sessions
  while plugged into a charger/USB: mains-referenced noise and (worst case) a
  fault path are exactly what you must avoid once electrodes touch skin.
- The XIAO's **onboard charger** fills the LiPo over USB; a slide switch in the
  BAT line gives a true off. Charge only when the device is **not** worn.
- Keep a single-point (star) ground. On the future board revision, give the
  analog/EEG section its own quiet ground pour tied to digital ground at one
  point. EEG signals are microvolts — layout noise discipline decided now is
  far cheaper than a respin later.

Regulator choice:
- The XIAO has an **onboard 3.3 V regulator** fed from the BAT pad, so no
  external regulator is needed for the prototype.
- For the future custom board, a low-noise LDO on the analog rail (feeding a
  future EEG front-end) beats a switching buck for signal quality.

---

## 10. Build order (de-risk)

1. XIAO ESP32-S3 + PCM5102 only. Get a binaural-beat tone out the jack.
2. Add D1/D2 PWM LEDs. Phase-lock light to the audio beat.
3. Bring up the WiFi web app (AP + home-WiFi join + fallback) and SW1 stop.
   Pick and run a full down-to-theta-and-back sequence from the browser.
4. Add MAX30102 on the ear. Show live BPM in the web UI — read only, no
   control yet.
5. (Stage 2) Close the loop: HR/HRV nudges the session.
6. (Stage 3) Spin a custom ESP32-S3 PCB and populate SPI with an EEG front-end.

---

## 11. EEG-compatibility checklist (the "don't box us out" promise)

- [x] MCU is ESP32-S3 (has the I2S + SPI + GPIO + BLE for all three stages).
- [x] PPG on I2C, EEG on SPI — separate buses, no contention at the chip level.
- [x] Battery-only operation path; charging isolated from wear.
- [x] Star ground now; dedicated analog ground pour planned for custom PCB.
- [x] Headband mechanics leave room near forehead/behind-ear for future
      electrode pads (same ear zone the PPG uses).
- [!] The XIAO prototype has only 3 spare pads — not enough for a full SPI EEG
      front-end on top of Stage 1. This is expected: Stage 3 is a custom board
      spin where every pin is broken out. The promise holds at the chip level,
      not the XIAO pad level.
