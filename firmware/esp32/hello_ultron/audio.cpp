#include "audio.h"
#include <ESP_I2S.h>

// --- Wiring / format (identical to the verified mic_speaker_test) ---
static const int I2S_BCLK = 9;
static const int I2S_WS = 10;
static const int I2S_DOUT = 12;
static const int I2S_DIN = 11;
static const int SAMPLE_RATE = 16000;

// --- Voice detector tuning (frames are 20 ms) ---
// Measured in this room (mic units = raw >> 14): silence ~500-1000, speech ~2000-9000.
static const int FRAME_SAMPLES = 320;
static const int START_FRAMES = 6;      // 120 ms above threshold to start
static const int HANGOVER_FRAMES = 40;  // 800 ms below stop threshold to end
static const int MIN_VOICED_FRAMES = 15;  // < 300 ms of voice = click/clap, discard
static const int MAX_FRAMES = 600;      // 12 s hard cap per utterance
static const int KEEP_TAIL_FRAMES = 10; // keep 200 ms of the trailing silence
static const int PREROLL_FRAMES = 20;   // 400 ms ring (includes the START frames)
static const int CALIBRATION_FRAMES = 50;  // 1 s of initial silence
static const int MIN_FLOOR = 200;
static const int MIN_START_THRESHOLD = 1200;
static const int MIN_STOP_THRESHOLD = 800;
static const int HOLDOFF_MS = 300;      // ignore mic this long after playback

// --- Capture / playback ---
static const int MAX_UTTERANCE_SAMPLES = MAX_FRAMES * FRAME_SAMPLES;  // 12 s = 384 KB, PSRAM
static const int FALLBACK_CAPTURE_SAMPLES = SAMPLE_RATE * 4;           // no PSRAM: 4 s of internal RAM
static const int REPLAY_GAIN_X10 = 30;      // x3 for the quiet mic capture, picked by ear
static const int16_t REPLAY_LIMIT = 12000;
static const int PCM_GAIN_X10 = 10;         // CEREBRO's TTS is already loudness-normalized
static const int16_t PCM_LIMIT = 16000;     // hard ceiling for anything sent to the amp
static const int PLAY_CHUNK_FRAMES = 256;

struct Frame {
  int32_t left;
  int32_t right;
};

enum PlayRequest { PLAY_NONE, PLAY_BEEP, PLAY_BUFFER };
enum DetectorState { DET_IDLE, DET_SPEECH };

static I2SClass i2s;
static TaskHandle_t audioTaskHandle = nullptr;

// Big buffers are static (task stack stays small).
static int16_t *capture = nullptr;  // allocated in audioInit (PSRAM if available)
static int captureCapacity = 0;
static int16_t preroll[PREROLL_FRAMES * FRAME_SAMPLES];
static Frame rxFrames[FRAME_SAMPLES];
static Frame txFrames[PLAY_CHUNK_FRAMES];

// Shared with the main loop (written by the task, or vice versa).
static volatile int st_level = 0;
static volatile int st_floor = MIN_FLOOR;
static volatile int st_startThr = MIN_START_THRESHOLD;
static volatile int st_stopThr = MIN_STOP_THRESHOLD;
static volatile bool st_calibrated = false;
static volatile bool st_speech = false;
static volatile bool st_playing = false;
static volatile uint32_t st_utterances = 0;
static volatile uint32_t st_lastUtteranceMs = 0;
static volatile uint32_t st_capturedSamples = 0;
static volatile int playRequest = PLAY_NONE;
static const int16_t *playPtr = nullptr;
static uint32_t playLen = 0;
static int playGainX10 = 10;
static int16_t playLimit = 16000;
static volatile bool st_deaf = false;             // ignore the mic (thinking / speaking)
static volatile bool deafAfterUtterance = false;  // go deaf the instant an utterance ends

// Tiny single-producer (task) / single-consumer (main loop) event queue.
static const int EVENT_QUEUE_SIZE = 8;
static volatile AudioEvent eventQueue[EVENT_QUEUE_SIZE];
static volatile uint8_t eventHead = 0;
static volatile uint8_t eventTail = 0;

static void pushEvent(AudioEvent e) {
  uint8_t next = (eventHead + 1) % EVENT_QUEUE_SIZE;
  if (next == eventTail) return;  // full: drop (main loop is way behind)
  eventQueue[eventHead] = e;
  eventHead = next;
}

// ---------------------------------------------------------------- playback

static void writeSample(int32_t s16, int &fill) {
  txFrames[fill].left = s16 << 16;
  txFrames[fill].right = s16 << 16;
  if (++fill == PLAY_CHUNK_FRAMES) {
    i2s.write((uint8_t *)txFrames, sizeof(txFrames));
    fill = 0;
  }
}

static void flushTx(int &fill) {
  if (fill > 0) {
    memset(&txFrames[fill], 0, (PLAY_CHUNK_FRAMES - fill) * sizeof(Frame));
    i2s.write((uint8_t *)txFrames, sizeof(txFrames));
    fill = 0;
  }
}

static void doPlayback(int request) {
  int fill = 0;
  if (request == PLAY_BEEP) {
    for (int n = 0; n < SAMPLE_RATE / 5; n++) {  // 200 ms, 880 Hz
      writeSample((int32_t)(3000 * sinf(2.0f * PI * 880.0f * n / SAMPLE_RATE)), fill);
    }
  } else if (request == PLAY_BUFFER) {
    const int16_t *pcm = playPtr;
    for (uint32_t n = 0; n < playLen; n++) {
      int32_t v = ((int32_t)pcm[n] * playGainX10) / 10;
      writeSample(constrain(v, -(int32_t)playLimit, (int32_t)playLimit), fill);
    }
  }
  flushTx(fill);
}

// ---------------------------------------------------------------- detector

static void audioTask(void *) {
  DetectorState state = DET_IDLE;
  int calibFrames = 0;
  int64_t calibSum = 0;
  int floorLevel = MIN_FLOOR;
  int aboveCount = 0;
  int silenceCount = 0;
  int voicedFrames = 0;
  int totalFrames = 0;
  int capLen = 0;
  int voicedEndLen = 0;  // capLen right after the last frame that had voice
  uint32_t holdoffUntil = 0;
  uint32_t speechStartMs = 0;
  int prerollPos = 0;    // next frame slot in the ring
  int prerollFilled = 0;

  for (;;) {
    int request = playRequest;
    if (request != PLAY_NONE) {
      if (state == DET_SPEECH) {
        state = DET_IDLE;
        st_speech = false;
        pushEvent(AUDIO_EVENT_SPEECH_ABORT);
      }
      st_playing = true;
      doPlayback(request);
      st_playing = false;
      playRequest = PLAY_NONE;
      holdoffUntil = millis() + HOLDOFF_MS;
      aboveCount = 0;
      prerollFilled = 0;
    }

    size_t got = i2s.readBytes((char *)rxFrames, sizeof(rxFrames));
    if (got < sizeof(rxFrames)) continue;
    if (st_deaf || (int32_t)(millis() - holdoffUntil) < 0) {  // deaf: thinking/speaking/just played
      aboveCount = 0;
      prerollFilled = 0;
      continue;
    }

    // DC-removed 18-bit-ish samples (same units as inmp441_test) and their RMS.
    int64_t sum = 0;
    for (int i = 0; i < FRAME_SAMPLES; i++) sum += rxFrames[i].left >> 14;
    int32_t dc = (int32_t)(sum / FRAME_SAMPLES);
    int64_t sumSq = 0;
    int16_t frame16[FRAME_SAMPLES];
    for (int i = 0; i < FRAME_SAMPLES; i++) {
      int32_t s = (rxFrames[i].left >> 14) - dc;
      sumSq += (int64_t)s * s;
      frame16[i] = (int16_t)constrain(s / 4, -32768, 32767);  // 18 -> 16 bit
    }
    int level = (int)sqrt((double)sumSq / FRAME_SAMPLES);
    st_level = level;

    if (!st_calibrated) {
      calibSum += level;
      if (++calibFrames >= CALIBRATION_FRAMES) {
        floorLevel = max(MIN_FLOOR, (int)(calibSum / calibFrames));
        st_calibrated = true;
      }
      st_floor = max(MIN_FLOOR, (int)(calibSum / calibFrames));
      continue;
    }

    int startThr = max(floorLevel * 35 / 10, MIN_START_THRESHOLD);
    int stopThr = max(floorLevel * 20 / 10, MIN_STOP_THRESHOLD);
    st_startThr = startThr;
    st_stopThr = stopThr;

    if (state == DET_IDLE) {
      memcpy(&preroll[prerollPos * FRAME_SAMPLES], frame16, sizeof(frame16));
      prerollPos = (prerollPos + 1) % PREROLL_FRAMES;
      if (prerollFilled < PREROLL_FRAMES) prerollFilled++;

      if (level > startThr) {
        aboveCount++;
      } else {
        aboveCount = 0;
        floorLevel += (level - floorLevel) / 50;  // slow adaptation, only in silence
        floorLevel = max(MIN_FLOOR, floorLevel);
        st_floor = floorLevel;
      }

      if (aboveCount >= START_FRAMES) {
        // Copy the ring (oldest first) into the capture buffer as pre-roll.
        capLen = 0;
        int first = (prerollFilled < PREROLL_FRAMES) ? 0 : prerollPos;
        for (int k = 0; k < prerollFilled; k++) {
          int slot = (first + k) % PREROLL_FRAMES;
          memcpy(&capture[capLen], &preroll[slot * FRAME_SAMPLES],
                 FRAME_SAMPLES * sizeof(int16_t));
          capLen += FRAME_SAMPLES;
        }
        state = DET_SPEECH;
        st_speech = true;
        voicedFrames = aboveCount;
        voicedEndLen = capLen;
        totalFrames = aboveCount;
        silenceCount = 0;
        speechStartMs = millis();
        pushEvent(AUDIO_EVENT_SPEECH_START);
      }
    } else {  // DET_SPEECH
      if (capLen + FRAME_SAMPLES <= captureCapacity) {
        memcpy(&capture[capLen], frame16, sizeof(frame16));
        capLen += FRAME_SAMPLES;
      }
      totalFrames++;
      if (level > stopThr) {
        silenceCount = 0;
        voicedFrames++;
        voicedEndLen = capLen;
      } else {
        silenceCount++;
      }

      if (silenceCount >= HANGOVER_FRAMES || totalFrames >= MAX_FRAMES) {
        state = DET_IDLE;
        st_speech = false;
        aboveCount = 0;
        prerollFilled = 0;
        if (voicedFrames < MIN_VOICED_FRAMES) {
          pushEvent(AUDIO_EVENT_SPEECH_ABORT);
        } else {
          // Keep a short silent tail after the last voiced frame; if the buffer filled
          // up earlier, capLen is already the limit.
          st_capturedSamples = (uint32_t)min(capLen, voicedEndLen + KEEP_TAIL_FRAMES * FRAME_SAMPLES);
          st_lastUtteranceMs = millis() - speechStartMs;
          st_utterances = st_utterances + 1;
          if (deafAfterUtterance) st_deaf = true;  // protect the capture buffer while CEREBRO uses it
          pushEvent(AUDIO_EVENT_SPEECH_END);
        }
      }
    }
  }
}

// ------------------------------------------------------------------ public

void audioInit() {
  capture = (int16_t *)ps_malloc(MAX_UTTERANCE_SAMPLES * sizeof(int16_t));
  captureCapacity = MAX_UTTERANCE_SAMPLES;
  if (!capture) {  // build without PSRAM=opi, or PSRAM failed
    capture = (int16_t *)malloc(FALLBACK_CAPTURE_SAMPLES * sizeof(int16_t));
    captureCapacity = FALLBACK_CAPTURE_SAMPLES;
    Serial.println("[audio] no PSRAM: utterances limited to 4 s (build with PSRAM=opi)");
  }
  if (!capture) {
    Serial.println("[audio] capture buffer allocation failed - audio disabled");
    return;
  }
  // Pins: BCLK, WS, DOUT, DIN, MCLK (not connected)
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, -1);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_STEREO)) {
    Serial.println("[audio] I2S init failed - audio disabled");
    return;
  }
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 3, &audioTaskHandle, 0);
}

AudioEvent audioPollEvent() {
  if (eventTail == eventHead) return AUDIO_EVENT_NONE;
  AudioEvent e = eventQueue[eventTail];
  eventTail = (eventTail + 1) % EVENT_QUEUE_SIZE;
  return e;
}

AudioStatus audioGetStatus() {
  AudioStatus s;
  s.level = st_level;
  s.noiseFloor = st_floor;
  s.startThreshold = st_startThr;
  s.stopThreshold = st_stopThr;
  s.calibrated = st_calibrated;
  s.speechActive = st_speech;
  s.playing = st_playing;
  s.utterances = st_utterances;
  s.lastUtteranceMs = st_lastUtteranceMs;
  s.capturedSamples = st_capturedSamples;
  return s;
}

bool audioIsPlaying() {
  return st_playing || playRequest != PLAY_NONE;
}

bool audioBeep() {
  if (!audioTaskHandle || audioIsPlaying()) return false;
  playRequest = PLAY_BEEP;
  return true;
}

static bool requestBuffer(const int16_t *pcm, uint32_t samples, int gainX10, int16_t limit) {
  if (!audioTaskHandle || audioIsPlaying() || !pcm || samples == 0) return false;
  playPtr = pcm;
  playLen = samples;
  playGainX10 = gainX10;
  playLimit = limit;
  playRequest = PLAY_BUFFER;  // set last: the task reads the fields above after seeing this
  return true;
}

bool audioReplayLastUtterance() {
  return requestBuffer(capture, st_capturedSamples, REPLAY_GAIN_X10, REPLAY_LIMIT);
}

bool audioPlayPcm16(const int16_t *pcm, size_t samples) {
  return requestBuffer(pcm, (uint32_t)samples, PCM_GAIN_X10, PCM_LIMIT);
}

const int16_t *audioCapturedPcm(size_t *samples) {
  *samples = st_capturedSamples;
  return capture;
}

void audioSetDeaf(bool deaf) {
  st_deaf = deaf;
}

void audioDeafAfterUtterance(bool enable) {
  deafAfterUtterance = enable;
}
