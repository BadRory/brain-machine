# Brain Machine — Firmware (Stage 1)

PlatformIO / Arduino firmware for the XIAO ESP32-S3. Fork of Mitch Altman's
Brain Machine / Sound & Light Machine (CC BY-SA).

## The core idea

Light and binaural audio are driven from **one timebase** so they can never
drift apart — the whole entrainment effect depends on that lock. The audio
task is the clock: each I2S block it computes the current entrainment `hz`
(from the session's fade curve), synthesizes a stereo block whose **L/R
carrier gap is that hz** (the binaural beat), and sets the LED PWM duty from
the **same** entrainment phase. The LED flash and the audio beat are the same
oscillator read twice. (This is also why audio is wired, not Bluetooth —
BT's 100–250 ms variable latency would break the lock.)

## Modules

| File | Role |
|------|------|
| `src/config.h` | pin map (XIAO silk → GPIO), audio params, **safety caps** |
| `src/session.{h,cpp}` | session/step data model + built-in presets (incl. the classic original) |
| `src/engine.{h,cpp}` | the phase-locked light + binaural audio core |
| `src/net.{h,cpp}` | WiFi: join saved home network, else **hotspot fallback** |
| `src/store.{h,cpp}` | LittleFS: user sessions, history, wifi creds |
| `src/webui.{h,cpp}` | self-hosted control API + static UI + captive portal + WS |
| `src/main.cpp` | boot order + the one hardware stop button |

## Safety (enforced in firmware, not optional)

See `config.h`: hard flash-rate cap (`SAFE_FLASH_HZ_MAX`), brightness cap,
soft-start ramp, a hard session time limit, and an awareness of the
photosensitive 15–25 Hz band. The stop button ends any running session
immediately. Run on battery only while worn. **Not a medical device.**

## Build

```
pio run                 # compile for seeed_xiao_esp32s3
pio run -t upload       # flash
pio run -t uploadfs     # upload the /data LittleFS image (UI assets)
pio device monitor      # serial @ 115200
```

Static UI assets go in `firmware/data/ui/` (served at `/ui`, with `/` →
`/ui/index.html`). Until uploaded, the device serves a minimal inline page and
the JSON API is live at `/api/status`.

## Control / API

Full plan: [`CONTROL_WEBAPP.md`](CONTROL_WEBAPP.md). Key endpoints:
`GET /api/sessions`, `GET /api/status`, `POST /api/run`, `POST /api/stop`,
`POST /api/sessions` (save), `GET /api/history`, `GET|POST /api/wifi`,
WebSocket `/ws` for live timer/hz/BPM.
