// Standalone INMP441 I2S microphone bench test (ESP32-S3).
// Wiring: VDD->3.3V, GND->GND, SCK->GPIO9, WS->GPIO10, SD->GPIO11, L/R->GND (left slot).
// Prints a volume bar + RMS/peak 10x per second. Clap or talk near the mic and the
// bar should jump; in a quiet room it should sit near zero.
// delay()/blocking read is fine here: isolated bench sketch, not the robot loop.
#include <ESP_I2S.h>

const int MIC_SCK = 9;   // I2S BCLK
const int MIC_WS = 10;   // I2S LRCLK / WS
const int MIC_SD = 11;   // I2S data in

const int SAMPLE_RATE = 16000;
const int SAMPLES_PER_REPORT = SAMPLE_RATE / 10;  // 100 ms of audio

I2SClass i2s;

void setup() {
  Serial.begin(115200);
  delay(1500);  // let the USB CDC console attach

  // Pins: BCLK, WS, DOUT (unused), DIN, MCLK (INMP441 needs none)
  i2s.setPins(MIC_SCK, MIC_WS, -1, MIC_SD, -1);

  // INMP441 sends 24-bit data MSB-aligned in a 32-bit slot; L/R=GND => left slot.
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S init failed");
    while (true) delay(1000);
  }
  Serial.println("INMP441 ready");
}

void loop() {
  int64_t sumSq = 0;
  int32_t peak = 0;
  int got = 0;

  while (got < SAMPLES_PER_REPORT) {
    int32_t raw;
    if (i2s.readBytes((char *)&raw, sizeof(raw)) != sizeof(raw)) continue;
    int32_t s = raw >> 14;  // keep ~18 significant bits; plenty for a level meter
    sumSq += (int64_t)s * s;
    int32_t a = s < 0 ? -s : s;
    if (a > peak) peak = a;
    got++;
  }

  int32_t rms = (int32_t)sqrt((double)sumSq / got);
  int bars = constrain(rms / 200, 0, 40);

  Serial.printf("rms %6ld peak %6ld |", (long)rms, (long)peak);
  for (int i = 0; i < bars; i++) Serial.print('#');
  Serial.println();
}
