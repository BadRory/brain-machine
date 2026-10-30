# Brain Machine — Aesthetic Display Subsystem (second ESP32)

Two round outward-facing LCDs behind the eye cups show visual feedback to an
observer. They are **pure art**. They are driven by a **second, separate
ESP32**, isolated from the audio/visual meditation path.

Credit: part of the fork of Mitch Altman's Brain Machine (CC BY-SA).

---

## 1. Why a second ESP32 (the hard rule)

The light + binaural-audio pulse must stay phase-locked. A screen refresh
cannot hold that timing, and screen drawing in a main loop steals time. So the
screens never share the chip that runs the pulse.

| Chip | Job | Timing-critical? |
|------|-----|------------------|
| ESP32 #1 (XIAO ESP32-S3) | LEDs + binaural audio + ear PPG + web app | **yes — the pulse** |
| ESP32 #2 | two round LCDs, observer art | no — looks only |

If #2 lags or crashes, the meditation session on #1 is untouched. That
isolation is the whole point.

**The red LEDs stay the entrainment light source.** The screens are never the
pulse light — a screen's refresh cannot match an LED's instant, exact on/off.

---

## 2. Displays

- 2 × **GC9A01** round LCD, 240 × 240, SPI. The cheap "smartwatch" round
  panels on AliExpress.
- Mounted behind the eye cups, **facing outward** (observer side), not at the
  wearer's eyes.

| GC9A01 pin | Connect to ESP32 #2 | Note |
|------------|---------------------|------|
| VCC | 3V3 | |
| GND | GND | |
| SCL (SCLK) | shared SPI clock | both screens share |
| SDA (MOSI) | shared SPI data | both screens share |
| DC | shared | data/command |
| RST | shared | reset (or tie to one pin) |
| CS | **one per screen** | the only non-shared line |
| BLK | 3V3 / PWM pin | backlight, optional dimming |

Two screens on one SPI bus: share SCLK/MOSI/DC/RST, give each its own **CS**.

### Pin budget on ESP32 #2

~6 pins: SCLK, MOSI, DC, RST, CS-left, CS-right (+ optional BLK PWM). Any XIAO
-class ESP32 has room to spare — #2 does nothing else.

---

## 3. Link between the two chips (optional)

A single **one-wire cue** so the art can react to the session. Loose timing
only — it can never break the pulse.

- Physical: one GPIO from #1 to #2 (simple), or a shared I2C/UART line.
- Content: slow, non-timing-critical cues — e.g. "now in theta", "heart rate =
  60 bpm", "session started / ended".
- #2 reacts in its animation; it never drives, gates, or waits on the pulse.

**Build it now, wire it dead.** Run the link physically but have #2 ignore it
at first (just loop its own animation). The door stays open to add reactive art
later with zero rework.

---

## 4. Power — the one real risk

Two chips + two always-on LCDs draw meaningfully more current, and screen
current spikes must not dip #1's audio rail. A noisy rail on #1 is the **one**
path by which the screens could ever reach the pulse — so close it.

- Give **each chip its own clean feed**: either two small flat LiPos, or one
  larger cell with a **separate regulator (LDO) per chip**, so #2's current
  spikes never sag #1's 3.3 V.
- Budget the battery for two always-on 240×240 panels; dim or sleep the LCDs
  during a session to save power and head heat.
- Battery off the face; keep the cell(s) in the enclosure bay behind the rigid
  plate, never against skin.

---

## 5. CAD impact (face plate)

- Two round LCD windows on the **front/outer** face, concentric with the eye
  cups (screens face out; red LEDs still face in at the eyes).
- Room in the electronics bay for a **second small board** plus its power rail.
- Plan the extra weight at the plate's center of mass, not on the nose.

See [`../cad/`](../cad/) for the face-plate model.

---

## 6. EEG future — unaffected

Moving the screens to #2 **frees** ESP32 #1's pins: the SPI bus the LCDs would
have eaten on #1 is no longer touched, so the Stage 3 EEG front-end path on #1
stays fully intact (see the EEG checklist in `STAGE1_HARDWARE.md`).
