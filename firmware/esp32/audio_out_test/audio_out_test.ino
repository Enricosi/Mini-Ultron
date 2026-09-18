// Standalone I2S audio-out bench test (ESP32-S3) for the Freenove audio converter/amp.
// Module: Freenove Audio Converter & Amplifier (single board: ES7148 DAC + PAM8403 amp).
// Wiring: VCC->5V, GND->common GND, BCK->GPIO9, LCK->GPIO10, DIN->GPIO12,
//         SCK/MCLK NOT connected (matches Freenove's official ESP32-S3 example),
//         speaker on the board's L+/L- output.
// BCK/LCK (GPIO9/10) are deliberately shared with the INMP441's SCK/WS: one I2S port,
// ESP32 is clock master for both. The mic's SD (GPIO11) is input-only and unused here.
// Plays a soft 440 Hz beep for 0.5 s, then 1.5 s of silence, forever.
// The speaker only ever connects to the amp board's speaker output, never to a GPIO.
// delay()/blocking write is fine here: isolated bench sketch, not the robot loop.
#include <ESP_I2S.h>

const int I2S_BCLK = 9;
const int I2S_WS = 10;
const int I2S_DOUT = 12;
const int I2S_MCLK = -1;  // not connected / disabled

const int SAMPLE_RATE = 16000;
const float TONE_HZ = 440.0;
const int16_t TONE_AMPLITUDE = 4000;  // ~12% of full scale: keep it gentle first

I2SClass i2s;

void writeStereoFrame(int16_t sample) {
  int16_t frame[2] = {sample, sample};  // same sample on L and R
  i2s.write((uint8_t *)frame, sizeof(frame));
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let the USB CDC console attach

  // Pins: BCLK, WS, DOUT, DIN (unused here), MCLK
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1, I2S_MCLK);

  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S init failed");
    while (true) delay(1000);
  }
  Serial.println("audio out ready: beep 0.5s / silence 1.5s");
}

void loop() {
  const int beepSamples = SAMPLE_RATE / 2;
  for (int n = 0; n < beepSamples; n++) {
    float phase = 2.0 * PI * TONE_HZ * n / SAMPLE_RATE;
    writeStereoFrame((int16_t)(TONE_AMPLITUDE * sinf(phase)));
  }
  const int silenceSamples = SAMPLE_RATE * 3 / 2;
  for (int n = 0; n < silenceSamples; n++) {
    writeStereoFrame(0);
  }
}
