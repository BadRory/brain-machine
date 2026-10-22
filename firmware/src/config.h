// config.h — board pin map, audio params, and SAFETY CAPS.
// Brain Machine (Stage 1). Fork of Mitch Altman's Brain Machine.
#pragma once

// ---- Pin map (per board, selected by the env's build flag) ----
#if defined(BM_BOARD_XIAO_S3)
// XIAO ESP32-S3 silk -> ESP32-S3 GPIO. Matches hardware/STAGE1_HARDWARE.md §4.
#define PIN_I2S_BCLK   2   // D1  -> PCM5102 BCK
#define PIN_I2S_LRCLK  3   // D2  -> PCM5102 LRCK
#define PIN_I2S_DOUT   4   // D3  -> PCM5102 DIN
#define PIN_I2C_SDA    5   // D4  -> MAX30102 SDA
#define PIN_I2C_SCL    6   // D5  -> MAX30102 SCL
#define PIN_LED_LEFT   1   // D0  -> left-eye LED (PWM)
#define PIN_LED_RIGHT  8   // D9  -> right-eye LED (PWM)
#define PIN_STOP_BTN   9   // D10 -> stop button (to GND, internal pull-up)

#elif defined(BM_BOARD_ESP32DEV)
// Classic ESP32-D / WROOM DevKit — dev board for UI work. Safe GPIOs chosen to
// avoid strapping/flash pins. Audio/PPG parts are NOT wired yet; these are
// placeholders so the firmware builds and the LED/button can be exercised.
#define PIN_I2S_BCLK   26  // free, I2S-capable
#define PIN_I2S_LRCLK  25
#define PIN_I2S_DOUT   22
#define PIN_I2C_SDA    21  // classic ESP32 default SDA
#define PIN_I2C_SCL    19
#define PIN_LED_LEFT   16  // onboard/dev LED-safe GPIOs
#define PIN_LED_RIGHT  17
#define PIN_STOP_BTN   4   // to GND, internal pull-up

#else
#error "No board selected: define BM_BOARD_XIAO_S3 or BM_BOARD_ESP32DEV in platformio.ini build_flags"
#endif

// ---- Audio ----
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BITS        16
#define AUDIO_FRAMES      256   // frames per I2S write block

// ---- LED PWM (LEDC) ----
#define LED_PWM_FREQ_HZ   20000 // 20 kHz carrier: silent, flicker-free dimming
#define LED_PWM_BITS      10    // 0..1023 duty resolution

// ---- SAFETY CAPS (enforced in firmware, never bypass) ----
// Flashing light can trigger photosensitive seizures. These are hard limits.
#define SAFE_FLASH_HZ_MAX     18.0f  // never flash faster than this
#define SAFE_FLASH_HZ_MIN      0.5f
#define SAFE_BRIGHTNESS_MAX    0.80f // fraction of full PWM duty
#define SAFE_SOFTSTART_S       3.0f  // ramp brightness in over N seconds at start
#define SAFE_SESSION_MAX_MIN   60    // hard stop after this long

// Photosensitive danger band: avoid *sustained* dwell here. Steps whose hz
// lands in this band are allowed to pass through during a fade but are clamped
// in dwell by the engine's soft limiter.
#define PHOTO_BAND_LO_HZ      15.0f
#define PHOTO_BAND_HI_HZ      25.0f

// ---- Networking ----
#define AP_SSID_PREFIX   "BrainMachine-"
#define MDNS_HOSTNAME    "brainmachine"
#define WIFI_JOIN_TIMEOUT_MS 9000

// ---- Storage ----
#define SESSIONS_DIR   "/sessions"   // user sequences (LittleFS)
#define HISTORY_FILE   "/history.json"
#define WIFI_CFG_FILE  "/wifi.json"
