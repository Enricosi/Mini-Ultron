#include "brain_link.h"
#include "brain_config.h"

#if CEREBRO_CONFIGURED

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

static const size_t REPLY_MAX_BYTES = 640 * 1024;  // 20 s of 16 kHz PCM16, in PSRAM
static const uint32_t POST_TIMEOUT_MS = 30000;     // STT + LLM + TTS on the server
static const uint32_t GET_TIMEOUT_MS = 10000;
static const uint32_t WIFI_WAIT_MS = 8000;

static volatile BrainState state = BRAIN_IDLE;
static BrainResult result;
static int16_t *replyBuf = nullptr;
static const int16_t *pendingPcm = nullptr;
static size_t pendingSamples = 0;
static bool serverChecked = false;

static String baseUrl() {
  return String("http://") + CEREBRO_HOST + ":" + CEREBRO_PORT;
}

static void fail(const char *why) {
  result.ok = false;
  strlcpy(result.error, why, sizeof(result.error));
  Serial.printf("[brain] error: %s\n", why);
}

static bool waitForWifi(uint32_t ms) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < ms) vTaskDelay(pdMS_TO_TICKS(100));
  return WiFi.status() == WL_CONNECTED;
}

// Called once per (re)connection so the very first thing you see is whether the
// server is reachable and what it is configured with.
static void checkServer() {
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(baseUrl() + "/api/health");
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      Serial.printf("[brain] server OK: llm=%s/%s stt=%s tts=%s key=%d movement_allowed=%d\n",
                    doc["llm"]["provider"] | "?", doc["llm"]["model"] | "?",
                    doc["stt"]["model"] | "?", doc["tts"]["voice"] | "?",
                    (int)(doc["llm"]["key_configured"] | false), (int)(doc["movement_allowed"] | false));
    }
  } else {
    Serial.printf("[brain] server %s:%d not reachable (http %d)\n", CEREBRO_HOST, CEREBRO_PORT, code);
  }
  http.end();
}

static bool downloadReply(const char *url) {
  HTTPClient http;
  http.setTimeout(GET_TIMEOUT_MS);
  http.begin(baseUrl() + url);
  int code = http.GET();
  if (code != 200) {
    http.end();
    fail("audio GET failed");
    return false;
  }
  int len = http.getSize();  // Content-Length, -1 if unknown
  size_t cap = REPLY_MAX_BYTES;
  size_t want = (len > 0 && (size_t)len < cap) ? (size_t)len : cap;
  if (len > (int)cap) {
    http.end();
    fail("reply audio too large");
    return false;
  }
  uint8_t *buf = (uint8_t *)replyBuf;
  WiFiClient *stream = http.getStreamPtr();
  size_t got = 0;
  uint32_t lastData = millis();
  while (got < want && (http.connected() || stream->available())) {
    int avail = stream->available();
    if (avail > 0) {
      got += stream->readBytes(buf + got, min((size_t)avail, want - got));
      lastData = millis();
    } else {
      if (millis() - lastData > GET_TIMEOUT_MS) break;
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
  http.end();
  if (got < 2 || (len > 0 && got < (size_t)len)) {
    fail("audio download incomplete");
    return false;
  }
  result.replySamples = got / 2;
  result.hasAudio = true;
  return true;
}

static void runRequest() {
  memset(&result, 0, sizeof(result));
  result.ok = true;

  if (!waitForWifi(WIFI_WAIT_MS)) {
    fail("wifi not connected");
    return;
  }

  uint32_t t0 = millis();
  HTTPClient http;
  http.setTimeout(POST_TIMEOUT_MS);
  http.begin(baseUrl() + "/api/converse");
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Session-Id", "mini-ultron");
  int code = http.POST((uint8_t *)pendingPcm, pendingSamples * sizeof(int16_t));
  if (code != 200) {
    http.end();
    fail(code > 0 ? "server returned an error" : "request failed (network/timeout)");
    Serial.printf("[brain] POST http code %d\n", code);
    return;
  }
  String body = http.getString();
  http.end();
  result.httpMs = millis() - t0;

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    fail("invalid JSON from server");
    return;
  }
  result.serverSttMs = doc["timings_ms"]["stt"] | 0;
  result.serverLlmMs = doc["timings_ms"]["llm"] | 0;
  result.serverTtsMs = doc["timings_ms"]["tts"] | 0;

  if (doc["ignored"] | false) {
    result.ignored = true;
    Serial.printf("[brain] server ignored the utterance: %s\n", doc["reason"] | "?");
    return;
  }
  Serial.printf("[brain] heard: \"%s\"\n", doc["transcript"] | "");
  Serial.printf("[brain] says: \"%s\"\n", doc["behavior"]["text"] | "");
  strlcpy(result.face, doc["behavior"]["face"] | "SPEAKING", sizeof(result.face));
  strlcpy(result.movement, doc["behavior"]["movement"] | "NONE", sizeof(result.movement));
  strlcpy(result.neck, doc["behavior"]["neck"] | "NONE", sizeof(result.neck));

  const char *url = doc["audio"]["url"] | (const char *)nullptr;
  if (url) {
    uint32_t t1 = millis();
    downloadReply(url);  // on failure result.ok=false, but face/neck are still usable
    result.downloadMs = millis() - t1;
  }
}

static void brainTask(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Modem power-save made the round trip ~100 ms and a 100 KB upload take ~9 s (TCP is
  // ACK-clocked). A wall-powered robot doesn't need it: keep the radio always on.
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  bool wasConnected = false;

  for (;;) {
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wasConnected) {
      Serial.printf("[brain] wifi connected, ip=%s\n", WiFi.localIP().toString().c_str());
      checkServer();
    } else if (!connected && wasConnected) {
      Serial.println("[brain] wifi lost, reconnecting");
    }
    wasConnected = connected;

    if (state == BRAIN_BUSY) {
      runRequest();
      state = BRAIN_DONE;
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

void brainInit() {
  replyBuf = (int16_t *)ps_malloc(REPLY_MAX_BYTES);
  if (!replyBuf) {
    state = BRAIN_DISABLED;
    Serial.println("[brain] disabled: no PSRAM for the reply buffer (build with PSRAM=opi)");
    return;
  }
  Serial.printf("[brain] enabled, server http://%s:%d\n", CEREBRO_HOST, CEREBRO_PORT);
  xTaskCreatePinnedToCore(brainTask, "brain", 12288, nullptr, 1, nullptr, 0);
}

BrainState brainState() { return state; }
bool brainWifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool brainSubmitUtterance(const int16_t *pcm, size_t samples) {
  if (state != BRAIN_IDLE || !pcm || samples == 0) return false;
  pendingPcm = pcm;
  pendingSamples = samples;
  state = BRAIN_BUSY;  // set last: the task reads the fields above after seeing this
  return true;
}

bool brainTakeResult(BrainResult *out) {
  if (state != BRAIN_DONE) return false;
  *out = result;
  state = BRAIN_IDLE;
  return true;
}

const int16_t *brainReplyPcm(size_t *samples) {
  *samples = result.replySamples;
  return replyBuf;
}

void brainPrintStatus() {
  Serial.printf("BRAIN: state=%d wifi=%d ip=%s server=%s:%d\n", (int)state, (int)brainWifiConnected(),
                WiFi.localIP().toString().c_str(), CEREBRO_HOST, CEREBRO_PORT);
}

#else  // ---- no wifi_config.h: inert, harmless ----

void brainInit() {
  Serial.println("[brain] disabled: copy wifi_config.example.h to wifi_config.h to enable CEREBRO");
}
BrainState brainState() { return BRAIN_DISABLED; }
bool brainWifiConnected() { return false; }
bool brainSubmitUtterance(const int16_t *, size_t) { return false; }
bool brainTakeResult(BrainResult *) { return false; }
const int16_t *brainReplyPcm(size_t *samples) { *samples = 0; return nullptr; }
void brainPrintStatus() { Serial.println("BRAIN: disabled (no wifi_config.h)"); }

#endif
