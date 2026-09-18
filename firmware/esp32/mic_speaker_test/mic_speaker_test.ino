// Standalone full-duplex I2S bench test (ESP32-S3): INMP441 mic + Freenove audio amp.
// Proves both directions coexist on ONE I2S port sharing BCLK/WS.
// Wiring (see docs/CURRENT_STATE.md): BCLK->GPIO9, WS->GPIO10, mic SD->GPIO11 (in),
// amp DIN->GPIO12 (out), no MCLK. Mic L/R->GND (left slot). Speaker on amp L+/L-.
// Cycle: "get ready" 2 s -> record 3 s -> play it back -> repeat.
// Playback gain x3 was picked by ear (Enrico, 2026-09-18) from an A/B run of x6 / x3 / x1:
// x6 amplified the mic's noise floor into an "old radio" hiss, x3 was the best balance,
// x1 was too quiet. Some radio-like tone remains: 16 kHz sampling limits bandwidth to 8 kHz.
// Record-then-play (not live loopback) on purpose: mic next to speaker would feed back.
// delay()/blocking I/O is fine here: isolated bench sketch, not the robot loop.
#include <ESP_I2S.h>

const int I2S_BCLK = 9;
const int I2S_WS = 10;
const int I2S_DOUT = 12;
const int I2S_DIN = 11;

const int SAMPLE_RATE = 16000;
const int RECORD_SECONDS = 3;
const int TOTAL_SAMPLES = SAMPLE_RATE * RECORD_SECONDS;

const int PLAYBACK_GAIN_X10 = 30;      // x3.0, in tenths
const int16_t PLAYBACK_LIMIT = 12000; // ~37% of full scale: keep speaker gentle

const int CHUNK_FRAMES = 256;

int16_t recording[TOTAL_SAMPLES];  // ~96 KB, fits in internal RAM
I2SClass i2s;

struct Frame {
  int32_t left;
  int32_t right;
};

void drainRx(int ms) {
  // Discard whatever piled up in the RX DMA while we were playing/idle.
  uint32_t end = millis() + ms;
  Frame f[CHUNK_FRAMES];
  while (millis() < end) {
    i2s.readBytes((char *)f, sizeof(f));
  }
}

void record() {
  Frame f[CHUNK_FRAMES];
  int n = 0;
  int64_t sum = 0;
  while (n < TOTAL_SAMPLES) {
    int want = min(CHUNK_FRAMES, TOTAL_SAMPLES - n);
    size_t got = i2s.readBytes((char *)f, want * sizeof(Frame));
    int frames = got / sizeof(Frame);
    for (int i = 0; i < frames; i++) {
      int16_t s = (int16_t)(f[i].left >> 16);  // top 16 bits of the 24-bit sample
      recording[n + i] = s;
      sum += s;
    }
    n += frames;
  }
  // Remove the mic's DC offset only; gain is applied per-version at playback time.
  int16_t dc = (int16_t)(sum / TOTAL_SAMPLES);
  for (int i = 0; i < TOTAL_SAMPLES; i++) {
    recording[i] = (int16_t)((int32_t)recording[i] - dc);
  }
}

void report() {
  int64_t sumSq = 0;
  int peak = 0;
  for (int i = 0; i < TOTAL_SAMPLES; i++) {
    int a = abs((int)recording[i]);
    sumSq += (int64_t)recording[i] * recording[i];
    if (a > peak) peak = a;
  }
  Serial.printf("recorded (raw, no gain): rms %d, peak %d\n",
                (int)sqrt((double)sumSq / TOTAL_SAMPLES), peak);
}

void play(int gainX10) {
  Frame f[CHUNK_FRAMES];
  int n = 0;
  while (n < TOTAL_SAMPLES) {
    int frames = min(CHUNK_FRAMES, TOTAL_SAMPLES - n);
    for (int i = 0; i < frames; i++) {
      int32_t v = ((int32_t)recording[n + i] * gainX10) / 10;
      v = constrain(v, -PLAYBACK_LIMIT, PLAYBACK_LIMIT);
      int32_t s = v << 16;
      f[i].left = s;
      f[i].right = s;
    }
    i2s.write((uint8_t *)f, frames * sizeof(Frame));
    n += frames;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let the USB CDC console attach

  // Pins: BCLK, WS, DOUT, DIN, MCLK (not connected)
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, -1);

  // Full duplex needs the same format both ways; 32-bit stereo suits the INMP441.
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S init failed");
    while (true) delay(1000);
  }
  Serial.println("full-duplex ready");
}

void loop() {
  Serial.println("get ready...");
  delay(2000);
  drainRx(300);
  Serial.println(">>> RECORDING 3 s: talk or clap now <<<");
  record();
  report();
  Serial.println(">>> PLAYING BACK <<<");
  play(PLAYBACK_GAIN_X10);
  Serial.println("done");
}
