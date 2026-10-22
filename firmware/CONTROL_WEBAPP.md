# Brain Machine — Control: WiFi Web App

The device is controlled from a web app it **hosts itself** over WiFi. No
encoder, no touch strip, no multi-click button. The only physical control is a
single stop button (SW1). This doc is the plan for that web app and the
networking behind it.

Credit: fork of Mitch Altman's Brain Machine (CC BY-SA). Keep attribution.

---

## 1. Why a web app

- **Zero GPIO cost.** The UI rides on the ESP32-S3 radio, not pins. That is
  what lets the design drop the encoder/touch and keep only a stop button.
- **Rich control.** Pick frequencies, browse ready sequences, build and order
  your own session, see history and a heart-rate chart — none of which a blind
  swipe can do.
- **Nothing extra to wear.** Your phone is the remote; the headband carries no
  screen or control surface beyond the stop button.
- **Setup/review, not live control.** During a session your eyes are shut, so
  the web app is for choosing and reviewing. In-session, the hardware stop
  button is the control.

---

## 2. Networking — AP + home-WiFi join, hotspot fallback

Standard IoT captive-provisioning pattern:

```
boot
 └─> saved home-WiFi creds?
       ├─ yes ─> try to join (timeout ~8–10 s)
       │          ├─ joined  ─> STA mode, reachable at brainmachine.local
       │          └─ failed  ─> fall back to hotspot (AP)
       └─ no  ─> hotspot (AP)

hotspot (AP):
  SSID "BrainMachine-XXXX", captive portal opens the web app
  user can enter home-WiFi creds in the UI -> saved to flash -> reboot & join
```

- **AP (hotspot):** the device makes its own WiFi. Works anywhere, no router.
  A captive portal pops the UI when the phone joins.
- **STA (joined home WiFi):** reachable at `brainmachine.local` via mDNS. Nicer
  for daily use at home.
- **Fallback:** if the saved network isn't found at boot (away from home,
  router down, wrong creds), it drops back to the hotspot so you are never
  locked out. Creds are entered/changed in the web UI, stored in NVS flash,
  and can be cleared (forget network) from the UI.

Libraries: `WiFiManager` or a hand-rolled `AsyncWebServer` + `DNSServer`
captive portal on Arduino/ESP-IDF. mDNS via `ESPmDNS`.

---

## 3. Web app screens

| Screen | What it does |
|--------|--------------|
| **Home** | Today's streak, "start your daily session" nudge, last session summary |
| **Library** | Browse frequencies (delta/theta/alpha/beta) and ready-made sequences |
| **Builder** | Add/order steps; each step = target Hz + minutes + fade; save as a named session |
| **Run** | Big Start, live timer + current Hz, live BPM, big on-screen Stop (mirrors SW1) |
| **History** | List of past sessions: date, length, sequence used, avg/min BPM, HRV trend |
| **Settings** | WiFi (join/forget), brightness cap, volume, safety limits, reminder time |

Served as a small single-page app (static HTML/CSS/JS in flash/LittleFS), talks
to the firmware over a tiny REST/JSON + WebSocket API.

---

## 4. API sketch (device <-> browser)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/sessions` | list built-in + user sequences |
| POST | `/api/sessions` | save a user-built sequence |
| POST | `/api/run` | start a session by id |
| POST | `/api/stop` | stop (same effect as SW1) |
| GET | `/api/history` | past sessions + HR summaries |
| GET | `/api/wifi` / POST `/api/wifi` | read status / set home creds |
| WS | `/ws` | live push: timer, current Hz, live BPM, state |

The WebSocket carries the live run so the Run screen's timer, frequency, and
heart rate update in real time while the session plays.

---

## 5. Session / sequence data model

A session is an ordered list of steps. Each step holds the entrainment target
and how long to dwell, with a fade into the next step.

```json
{
  "id": "evening-winddown",
  "name": "Evening wind-down",
  "steps": [
    { "hz": 15.0, "minutes": 2, "fade_s": 30 },
    { "hz": 10.0, "minutes": 4, "fade_s": 60 },
    { "hz": 6.0,  "minutes": 6, "fade_s": 60 },
    { "hz": 4.0,  "minutes": 4, "fade_s": 60 }
  ],
  "audio": { "carrier_hz": 200, "soundscape": "soft-rain", "volume": 0.6 },
  "light": { "brightness": 0.5, "pattern": "both" }
}
```

`hz` drives **both** the LED flash rate and the binaural beat (the L/R carrier
gap), kept phase-locked in firmware. The classic original is one such sequence
(beta -> theta -> back); it ships as a built-in preset alongside the user's own.

---

## 6. History, heart rate, streaks & rewards

- **Per session stored to flash:** date/time, sequence id, duration actually
  run, and the heart-rate track (BPM over time; inter-beat intervals for HRV).
- **History screen:** timeline of sessions with a small BPM/HRV chart per
  session, so the user can see themselves settle over the minutes.
- **Streaks & nudges:** count consecutive days with a completed session; show a
  daily reminder at a user-set time; celebrate milestones (3/7/30 days). Frame
  rewards around *showing up and calming down* (session done, HR dropped during
  the session), not medical claims.
- **Storage:** LittleFS/NVS on the chip. The phone browser keeps nothing, so
  history survives a new phone. A later phone-app or cloud sync can carry
  streaks across devices — not needed for Stage 1.

**Not a medical device.** Heart rate and HRV here are for feedback and
motivation, not diagnosis or treatment. No health claims.

---

## 7. Stage-fit notes

- WiFi web app uses the radio, not GPIO — the EEG future is untouched.
- The stop button stays hardware so the eyes-shut user is never dependent on a
  phone to end a session.
- History/HR plumbing added now is exactly what Stage 2 (HR/HRV biofeedback
  steering the live session) builds on.
