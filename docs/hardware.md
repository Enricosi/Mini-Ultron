# Mini-Ultron — Hardware (source of truth)

This is the technical inventory of Mini-Ultron's physical hardware: what exists, what
controls it, which pins/buses it uses, and how confident we are that it actually
works. It is meant to converge into the single place you check before touching any
wire or writing any firmware line that assigns a pin.

**Status legend** (same as `CURRENT_STATE.md`): **VERIFIED** (measured/confirmed
physically) · **PHYSICALLY TESTED** (run on real hardware) · **IMPLEMENTED** (code
exists) · **COMPILES** · **UPLOADED** · **UNTESTED** · **PLANNED** · **DEFERRED** ·
**NEEDS VERIFICATION**.

For the day-to-day narrative (what changed this session, open bugs, next steps) see
`CURRENT_STATE.md` — that file is the live log; this one is the settled inventory. If
they ever disagree, trust the code and `CURRENT_STATE.md`'s most recent entry, not
this file, and fix this file.

## Compute

| Component | Role | Status |
|---|---|---|
| Freenove ESP32-S3 WROOM (Ultimate Starter Kit) + GPIO Extension Board | Upper body: face, voice, Wi-Fi, UART bridge to the Pico | VERIFIED, in use |
| Raspberry Pi Pico W (part of the Freenove FNK0089 kit) | Lower body: locomotion, neck, sensors, local safety | VERIFIED, in use |
| Home-lab mini server (Ubuntu 24.04, off-robot) | CEREBRO: Django, STT, LLM client, TTS, personality | VERIFIED, deployed 2026-09-18 |

The ESP32 and Pico are the only microcontrollers on the robot. The mini server is not
physically on the robot; it talks to the ESP32 over Wi-Fi/HTTP.

## ESP32-S3 peripherals

| Component | Interface | Pins | Voltage | Status | Notes |
|---|---|---|---|---|---|
| SSD1331 96x64 OLED | Software SPI | SCLK=47, MOSI=21, RST=2, DC=41, CS=42 | 3.3V | VERIFIED, PHYSICALLY TESTED (9 face states) | `Adafruit_SSD1331`/`Adafruit_GFX` |
| UART to Pico | `Serial1`, 9600 baud | RX=17 (from Pico TX/GPIO0), TX=18 (to Pico RX/GPIO1) | 3.3V logic, common GND | VERIFIED, PHYSICALLY TESTED end-to-end | Only transport, permanent design |
| INMP441 I2S mic | I2S (shared port, full-duplex) | SCK/BCLK=9, WS/LRCLK=10, SD=11 | 3.3V (VDD), L/R→GND | WIRED, PHYSICALLY TESTED | Shares clocks with the amp |
| Freenove Audio Converter & Amplifier (ES7148 DAC + PAM8403 amp) | I2S (same port as mic) | BCK=9 (shared), LCK=10 (shared), DIN=12 | 5V (own VCC/GND) | WIRED, PHYSICALLY TESTED | SCK/MCLK intentionally not connected; speaker only on amp L+/L-, never on a GPIO |
| 8Ω 2W speaker | Analog, from amp board | — (amp L+/L- only) | — | WIRED, PHYSICALLY TESTED | Never wire directly to a GPIO |
| MPU6050-family IMU (WHO_AM_I=0x70, MPU6500 clone) | I2C, 100 kHz | SDA=13, SCL=14 | 5V (breakout), common GND | **DROPPED from v1 2026-10-02** (`MINI_ULTRON_ENABLE_IMU 0`, pins free); was WIRED, PHYSICALLY TESTED, but data UNRELIABLE while audio runs | I2S clocks on GPIO9/10 crosstalk into the I2C lines; software-mitigated, not fixed. Does not drive anything safety-relevant. |
| GPIO1, GPIO48 | I2C (formerly PCA9685 bus) | free | — | FREE, not reserved | PCA9685 physically removed from v1; don't reuse these for anything without checking `arm_gestures.h` first |
| GPIO16 | — | free | — | FREE, not reserved | Amp SCK/MCLK pad, intentionally not connected (matches Freenove's own reference design) |
| Camera (Freenove ESP32-S3 module) | MIPI/flex | — | — | DEFERRED (physically detached) | Not part of v1; don't mechanically stress the flex cable |
| PCA9685 + 2 arm servos | I2C / PWM | (historical: SDA=1, SCL=48) | — | REMOVED from v1 hardware | Code preserved as historical reference only (`arm_gestures.{h,cpp}`, `MINI_ULTRON_ENABLE_ARMS 0`) |

## Pico W / Freenove FNK0089 peripherals

| Component | Interface | Pins | Voltage | Status | Notes |
|---|---|---|---|---|---|
| 4x DC motor + Mecanum wheel (M1=front-left, M2=back-left, M3=back-right, M4=front-right) | PWM, H-bridge on shield | M1=18/19, M2=20/21, M3=6/7, M4=8/9 | motor rail (2x18650) | VERIFIED, PHYSICALLY TESTED all 4 wheels, all directions | Wheel mapping and all 4 `REVERSE_MOTORx` flags physically confirmed 2026-09-15 |
| Front/neck servo | PWM (vendored `Servo_1_Angle`) | GPIO13 | shield servo rail | IMPLEMENTED, PHYSICALLY TESTED (centers correctly) | Wired to the shield's own servo header — **not** the ESP32/PCA9685. Only 3 named positions reachable from firmware (`NECK_CENTER/LEFT/RIGHT`), never a raw angle |
| HC-SR04 ultrasonic | Trig/Echo | TRIG=4, ECHO=5 | 5V | **DROPPED from v1 2026-10-02** (`MINI_ULTRON_ENABLE_ULTRASONIC 0` in the Pico's `features.h`; no forward veto); code IMPLEMENTED for builds that have it | `safetyForwardBlocked()` only gates `MOVE:FWD` |
| 3-channel line tracker | Digital | LEFT=12, CENTER=11, RIGHT=10 | shield logic rail | IMPLEMENTED | Not currently used for any decision, only reported over `STATUS` |
| WS2812 status LEDs (8 LEDs) | Single-wire (Adafruit NeoPixel) | GPIO16 | shield 5V rail | IMPLEMENTED, COMPILES, UPLOADED, **PHYSICALLY VERIFIED 2026-09-21** (Enrico confirmed dim, correct colors, no full-brightness/white) | Own driver (`led_controller.{h,cpp}`), not vendored from Freenove's WS2812 helper. Mirrors the ESP32's 9 `FaceState` values, one fixed dim color each (`LED:FACE:<NAME>`), plus manual `LED:ON`/`LED:OFF` |
| 2x 18650 Li-ion cells | — | — | ~7.4V nominal | AVAILABLE, used for locomotion tests | Powers the shield/motors; ESP32 power source and rail sharing not yet fully verified — see "Open power questions" below |

Freenove's own vendored driver (`Freenove_4WD_Car_For_Pico_W.{h,cpp}`) additionally
supports a buzzer, an LED matrix, IR receiver, a photosensitive sensor, and an
"emotion board" — **none of that hardware is on Mini-Ultron's chassis**, and none of
those sections were vendored in. Don't assume they exist just because the upstream
kit supports them.

## Open power questions (NEEDS VERIFICATION)

- Whether the Freenove shield's 5V rail can safely power the ESP32 subsystem instead
  of/alongside its current supply — not measured. Do not connect until verified.
- Total current budget with motors + neck + LEDs + ESP32 all active simultaneously
  has not been measured under load.

## Communication summary (see `communications.md` for the protocol detail)

```
Django (home-lab)  --Wi-Fi, HTTP-->  ESP32-S3  --UART, 9600 baud-->  Pico W  -->  motors/servo/sensors/LEDs
```

## What would need to change to leave the Freenove shield behind

Notes for a future custom PCB, not a task to do now (see `THIRD_PARTY_LICENSES.md` for
the licensing side of the same question):

- The motor driver, servo header, ultrasonic header, and WS2812 pad are all on the
  Freenove shield — a custom board would need to replace all of them, not just the
  Pico module itself.
- The ESP32↔Pico contract is a plain ASCII UART protocol at 9600 baud — trivially
  portable to any two microcontrollers with a UART, not shield-specific. This is
  already a good boundary to freeze.
- Pin assignments on both sides are shield/board-specific today (e.g. Pico GPIO0/1 are
  fixed by the RP2040 module, not the shield) — a custom board is free to choose new
  ones as long as the protocol contract above doesn't change.
- The audio path (I2S full-duplex, mic+amp sharing clocks) and the MPU6050 I2C-vs-I2S
  crosstalk issue are ESP32-board-level concerns that a custom PCB could fix properly
  (separate clock domains, better trace routing) instead of the current
  software-only mitigation.
