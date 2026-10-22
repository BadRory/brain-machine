# Brain Machine (modern fork)

An open-source, modern fork of Mitch Altman's **Sound & Light Machine** (the
"Brain Machine"). It blinks LEDs in glasses and plays tones in your ears, both
pulsing at brainwave frequencies, to guide meditative / relaxed states through
photic + auditory entrainment.

This fork moves from the original ATtiny to a **Seeed XIAO ESP32-S3** and adds:

- **Real stereo audio** (binaural beats + soundscapes) via an I2S DAC, instead
  of square-wave beeps.
- **A WiFi web app** the device hosts itself: pick frequencies, browse and
  build sequences, and review past sessions with heart-rate history and daily
  streaks. No encoder or touch needed — only a single stop button on the band.
- **Ear heart rate** (earlobe / behind-ear PPG) for history now and future
  biofeedback.
- A hardware design that **keeps a clean path to a future EEG front-end** so
  that upgrade never forces a redesign.

## Credit & license

Forked in spirit from Mitch Altman's Brain Machine, which is released under
**Creative Commons Attribution-ShareAlike**. This project keeps that license
(see `LICENSE`). Attribution to Mitch Altman is required; derivatives stay open.

- Original: https://learn.adafruit.com/brain-machine
- Mitch Altman's kit: https://github.com/maltman23/Brain_Machine_kit

## Safety

Flashing lights can trigger seizures in photosensitive people. This project
enforces a brightness cap, a flash-frequency clamp, soft-start ramps, and a
physical stop button. Run on battery only while worn. This is a DIY wellness /
art project, **not a medical device** — no diagnosis, no treatment claims.

## Roadmap

- **Stage 1** — Modern core: XIAO ESP32-S3 + I2S audio + PWM glasses LEDs + ear
  PPG, controlled by a self-hosted WiFi web app.
  (hardware: [`hardware/STAGE1_HARDWARE.md`](hardware/STAGE1_HARDWARE.md) ·
  control: [`firmware/CONTROL_WEBAPP.md`](firmware/CONTROL_WEBAPP.md))
- **Stage 2** — Heart-rate / HRV biofeedback steers the session.
- **Stage 3** — Optional EEG front-end (ADS1299 / open ESP-EEG) on the reserved
  SPI bus.

## Status

Early design. Stage 1 hardware & wiring documented; firmware next.
