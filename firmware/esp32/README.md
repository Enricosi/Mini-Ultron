# ESP32-S3 firmware

`hello_ultron/` is **the** firmware. Everything else here is a single-purpose bench sketch
(`delay()` is allowed in these, never in `hello_ultron`).

| Sketch | For | Status |
|---|---|---|
| `hello_ultron/` | the robot: face, audio, CEREBRO link, Pico bridge | V1 |
| `mic_speaker_test/`, `inmp441_test/`, `audio_out_test/` | mic / amp wiring checks on GPIO9/10/11/12 | V1 bench tools |
| `i2c_scan/` | I2C bus scanner (set the pins in the sketch) | bench tool |
| `mpu6050_test/` | IMU bench | deferred hardware (IMU not in V1) |
| `servo_test/` | PCA9685 + arm servos bench | deferred hardware, removed from the robot; not runnable on the current wiring |

Compile: `arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" firmware/esp32/hello_ultron`
Optional modules are switched in `hello_ultron/features.h`.
