#include "conversation.h"
#include "audio.h"
#include "brain_link.h"
#include "face.h"
#include "imu.h"
#include "motion.h"
#include "pico_link.h"

static const unsigned long HOLD_AFTER_SPEECH_MS = 1500;

enum ConvState { CV_IDLE, CV_LISTENING, CV_THINKING, CV_SPEAKING, CV_HOLD };

static ConvState state = CV_IDLE;
static FaceState faceBefore = FACE_IDLE;
static unsigned long holdUntil = 0;
static bool wasPlaying = false;
static unsigned long lastReactedJoltMs = 0;  // dedupes imuGetData().lastJoltMs
static char pendingAnim[25] = "";  // animation ordered this turn, played once the reply is spoken

// Turn timing (printed once per turn so latency can be attacked later).
static unsigned long tSpeechEnd = 0, tPlayStart = 0;
static BrainResult lastResult;

// Only the faces CEREBRO may choose (LISTENING/THINKING are ours).
static bool faceFromName(const char *name, FaceState *out) {
  if (!strcmp(name, "IDLE")) *out = FACE_IDLE;
  else if (!strcmp(name, "ANGRY")) *out = FACE_ANGRY;
  else if (!strcmp(name, "SURPRISED")) *out = FACE_SURPRISED;
  else if (!strcmp(name, "SUSPICIOUS")) *out = FACE_SUSPICIOUS;
  else if (!strcmp(name, "SPEAKING")) *out = FACE_SPEAKING;
  else if (!strcmp(name, "GLITCH")) *out = FACE_GLITCH;
  else if (!strcmp(name, "SLEEPING")) *out = FACE_SLEEPING;
  else return false;
  return true;
}

static void applyNeck(const char *name) {
  if (!strcmp(name, "CENTER")) picoNeckCenter();
  else if (!strcmp(name, "LEFT")) picoNeckLeft();
  else if (!strcmp(name, "RIGHT")) picoNeckRight();
  // NONE / anything else: leave the neck alone
}

// CEREBRO sends names like "CALABAZA"; built-in animations are lowercase. "NONE",
// empty or anything facePlayAnimation() doesn't know is simply not played.
static void queueAnimation(const char *name) {
  pendingAnim[0] = '\0';
  if (!strcmp(name, "NONE")) return;
  size_t i = 0;
  for (; name[i] && i < sizeof(pendingAnim) - 1; i++) pendingAnim[i] = tolower((unsigned char)name[i]);
  pendingAnim[i] = '\0';
}

static void finishTurn(unsigned long now, FaceState face, unsigned long holdMs) {
  setFace(face);
  holdUntil = now + holdMs;
  state = CV_HOLD;
}

static void onBrainResult(unsigned long now, const BrainResult &r) {
  lastResult = r;
  if (r.ignored) {
    setFace(faceBefore);
    audioSetDeaf(false);
    state = CV_IDLE;
    return;
  }
  if (r.face[0] == '\0') {  // nothing usable came back at all
    finishTurn(now, FACE_GLITCH, HOLD_AFTER_SPEECH_MS);
    return;
  }

  FaceState face = FACE_SPEAKING;
  faceFromName(r.face, &face);
  setFace(face);
  applyNeck(r.neck);
  if (r.ok) motionStart(motionFromName(r.movement));  // a partial/failed turn never moves
  if (r.ok) queueAnimation(r.animation);

  size_t samples = 0;
  const int16_t *pcm = brainReplyPcm(&samples);
  if (r.hasAudio && audioPlayPcm16(pcm, samples)) {
    tPlayStart = now;
    state = CV_SPEAKING;
  } else {
    holdUntil = now + 2 * HOLD_AFTER_SPEECH_MS;  // no audio: still show the expression
    state = CV_HOLD;
  }
}

bool conversationTriggerEvent(const char *eventName) {
  if (state != CV_IDLE) return false;
  if (!brainSubmitEvent(eventName)) return false;
  faceBefore = getFace();
  setFace(FACE_THINKING);
  state = CV_THINKING;
  Serial.printf("[conversation] event %s -> CEREBRO\n", eventName);
  return true;
}

void conversationInit() {
  audioDeafAfterUtterance(brainState() != BRAIN_DISABLED);
}

void conversationUpdate(unsigned long now) {
  // A jolt is a physical event, not speech: only report it while otherwise idle, so it
  // never interrupts a turn already in progress. imu.cpp already debounces/cooldowns the
  // jolt itself; lastReactedJoltMs just stops us from reporting the same one twice.
  if (state == CV_IDLE) {
    unsigned long joltMs = imuGetData().lastJoltMs;
    if (joltMs != 0 && joltMs != lastReactedJoltMs) {
      lastReactedJoltMs = joltMs;
      conversationTriggerEvent("JOLT");
    }
  }

  AudioEvent e;
  while ((e = audioPollEvent()) != AUDIO_EVENT_NONE) {
    if (e == AUDIO_EVENT_SPEECH_START && state == CV_IDLE) {
      faceBefore = getFace();
      motionAbort();  // never keep moving while someone is talking to us
      setFace(FACE_LISTENING);
      state = CV_LISTENING;
      Serial.println("[audio] speech start");
    } else if (e == AUDIO_EVENT_SPEECH_ABORT && state == CV_LISTENING) {
      setFace(faceBefore);
      state = CV_IDLE;
      Serial.println("[audio] speech ignored (too short/interrupted)");
    } else if (e == AUDIO_EVENT_SPEECH_END && state == CV_LISTENING) {
      AudioStatus a = audioGetStatus();
      Serial.printf("[audio] speech end: %u ms, %u samples captured\n", (unsigned)a.lastUtteranceMs,
                    (unsigned)a.capturedSamples);
      size_t samples = 0;
      const int16_t *pcm = audioCapturedPcm(&samples);
      if (brainSubmitUtterance(pcm, samples)) {
        tSpeechEnd = now;
        setFace(FACE_THINKING);
        state = CV_THINKING;
      } else {  // brain disabled or busy: old behavior
        setFace(faceBefore);
        audioSetDeaf(false);
        state = CV_IDLE;
      }
    }
  }

  switch (state) {
    case CV_THINKING: {
      BrainResult r;
      if (brainTakeResult(&r)) onBrainResult(now, r);
      break;
    }
    case CV_SPEAKING:
      if (!audioIsPlaying()) {
        AudioStatus a = audioGetStatus();
        Serial.printf(
            "[turn] utterance=%ums | server round trip=%ums (stt=%u llm=%u tts=%u) audio dl=%ums | "
            "speech-end->playback=%lums | playback=%lums\n",
            (unsigned)a.lastUtteranceMs, (unsigned)lastResult.httpMs, (unsigned)lastResult.serverSttMs,
            (unsigned)lastResult.serverLlmMs, (unsigned)lastResult.serverTtsMs,
            (unsigned)lastResult.downloadMs, tPlayStart - tSpeechEnd, now - tPlayStart);
        holdUntil = now + (pendingAnim[0] ? 0 : HOLD_AFTER_SPEECH_MS);  // dance right away
        state = CV_HOLD;
      }
      break;
    case CV_HOLD:
      if ((long)(now - holdUntil) >= 0) {
        setFace(faceBefore);
        audioSetDeaf(false);
        state = CV_IDLE;
        if (pendingAnim[0]) {
          // Plays once over the idle face; speaking to Ultron (LISTENING) cancels it.
          bool started = facePlayAnimation(pendingAnim, false);
          Serial.printf("[conversation] animation %s %s\n", pendingAnim, started ? "started" : "unknown, ignored");
          pendingAnim[0] = '\0';
        }
      }
      break;
    default:
      break;
  }

  // Debug beeps / AUDIO:REPLAY (not part of a conversation): show the talking face.
  bool playing = audioIsPlaying();
  if (state == CV_IDLE) {
    if (playing && !wasPlaying) {
      faceBefore = getFace();
      setFace(FACE_SPEAKING);
    } else if (!playing && wasPlaying) {
      setFace(faceBefore);
    }
  }
  wasPlaying = playing;
}
