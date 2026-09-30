"""
The whole CEREBRO turn: audio -> STT -> LLM -> validated Behavior -> TTS.

Failure policy (the robot should never just go silent):
- STT returns nothing / a Whisper hallucination -> the turn is "ignored", no reply.
- LLM fails or returns unusable output        -> canned fallback line + GLITCH face.
- TTS fails                                   -> behavior is still returned, without audio.
"""
import json
import logging
import threading
import time
import uuid
from collections import OrderedDict, defaultdict, deque
from dataclasses import dataclass, field

from . import audio_utils
from .llm import LLMBackend, LLMError
from .personalities import DEFAULT_PERSONALITY, Personality
from .schemas import Behavior, BehaviorError, apply_animation_policy, apply_policy, parse_behavior
from .stt import STTBackend, is_hallucination
from .tts import Speech, TTSBackend, TTSError

logger = logging.getLogger("brain.pipeline")

MIN_AUDIO_S = 0.3  # shorter than this is a click, not speech

# Physical events a controller can report instead of an utterance (see each personality's
# "EVENTOS FÍSICOS" prompt section, e.g. personalities/ultron.py). The firmware sends the
# bare name; unknown names are ignored, same policy as an unparseable LLM field elsewhere
# in this pipeline. Not personality-specific itself (yet) - one shared vocabulary.
EVENT_PROMPTS = {
    "JOLT": (
        "[EVENTO FÍSICO: alguien te acaba de agarrar o mover bruscamente, sin avisar, "
        "y lo sentiste con tu propio cuerpo] Reacciona con UNA frase corta y digna de "
        "villano: indignación, sospecha o advertencia. No preguntes qué pasó, ya lo sabes."
    ),
}


class AudioStore:
    """The last few generated replies, so the ESP32 can GET them by id."""

    def __init__(self, max_items: int = 8):
        self.max_items = max_items
        self._items: OrderedDict[str, Speech] = OrderedDict()
        self._lock = threading.Lock()

    def put(self, speech: Speech) -> str:
        audio_id = uuid.uuid4().hex[:12]
        with self._lock:
            self._items[audio_id] = speech
            while len(self._items) > self.max_items:
                self._items.popitem(last=False)
        return audio_id

    def get(self, audio_id: str) -> Speech | None:
        with self._lock:
            return self._items.get(audio_id)


class History:
    """Rolling short-term memory per session, in RAM (v0.1: no persistence)."""

    def __init__(self, max_turns: int = 6):
        self._turns = defaultdict(lambda: deque(maxlen=max_turns))
        self._lock = threading.Lock()

    def messages(self, session: str) -> list[dict]:
        with self._lock:
            turns = list(self._turns[session])
        out: list[dict] = []
        for user_text, behavior_json in turns:
            out.append({"role": "user", "content": user_text})
            out.append({"role": "assistant", "content": behavior_json})
        return out

    def add(self, session: str, user_text: str, behavior: Behavior) -> None:
        with self._lock:
            self._turns[session].append((user_text, json.dumps(behavior.to_dict(), ensure_ascii=False)))


@dataclass
class Turn:
    ignored: bool = False
    ignored_reason: str = ""
    transcript: str = ""
    behavior: Behavior | None = None
    audio_id: str | None = None
    audio: Speech | None = None
    timings_ms: dict = field(default_factory=dict)
    warnings: list = field(default_factory=list)


class Pipeline:
    def __init__(
        self,
        stt: STTBackend,
        llm: LLMBackend,
        tts: TTSBackend,
        history: History | None = None,
        audio_store: AudioStore | None = None,
        allow_movement: bool = False,
        max_reply_chars: int = 280,
        personality: Personality = DEFAULT_PERSONALITY,
    ):
        self.stt = stt
        self.llm = llm
        self.personality = personality
        self.tts = tts
        self.history = history or History()
        self.audio_store = audio_store or AudioStore()
        self.allow_movement = allow_movement
        self.max_reply_chars = max_reply_chars

    # ---- public entry points -------------------------------------------------

    def handle_audio(self, body: bytes, session: str = "default") -> Turn:
        t0 = time.perf_counter()
        turn = Turn()
        try:
            audio = audio_utils.decode_upload(body)
        except (ValueError, EOFError) as exc:
            return self._ignore(turn, f"bad_audio: {exc}", t0)
        if len(audio) / audio_utils.SAMPLE_RATE < MIN_AUDIO_S:
            return self._ignore(turn, "too_short", t0)

        t = time.perf_counter()
        transcript = self.stt.transcribe(audio_utils.normalize_peak(audio))
        turn.timings_ms["stt"] = _ms(t)
        if not transcript.text or is_hallucination(transcript.text):
            turn.transcript = transcript.text
            return self._ignore(turn, "no_speech", t0)
        turn.transcript = transcript.text
        return self._respond(turn, transcript.text, session, t0)

    def handle_text(self, text: str, session: str = "default") -> Turn:
        t0 = time.perf_counter()
        turn = Turn(transcript=text.strip())
        if not turn.transcript:
            return self._ignore(turn, "empty_text", t0)
        return self._respond(turn, turn.transcript, session, t0)

    def handle_event(self, event: str, session: str = "default") -> Turn:
        t0 = time.perf_counter()
        turn = Turn(transcript=f"[event:{event}]")
        prompt = EVENT_PROMPTS.get(event)
        if prompt is None:
            return self._ignore(turn, "unknown_event", t0)
        return self._respond(turn, prompt, session, t0, from_speech=False)

    # ---- internals ---------------------------------------------------------------

    def _ignore(self, turn: Turn, reason: str, t0: float) -> Turn:
        turn.ignored = True
        turn.ignored_reason = reason
        turn.timings_ms["total"] = _ms(t0)
        logger.info("turn ignored (%s) stt=%sms", reason, turn.timings_ms.get("stt", "-"))
        return turn

    def _respond(self, turn: Turn, user_text: str, session: str, t0: float, from_speech: bool = True) -> Turn:
        messages = [{"role": "system", "content": self.personality.system_prompt}]
        messages += self.history.messages(session)
        messages.append({"role": "user", "content": user_text})

        t = time.perf_counter()
        try:
            behavior, warnings = parse_behavior(self.llm.complete(messages), self.max_reply_chars)
            turn.warnings += warnings
        except (LLMError, BehaviorError) as exc:
            logger.warning("LLM/behavior failure, using fallback: %s", exc)
            turn.warnings.append(f"fallback: {exc}")
            behavior = Behavior(text=self.personality.fallback_text, face=self.personality.fallback_face)
        turn.timings_ms["llm"] = _ms(t)

        requested_movement = behavior.movement
        behavior = apply_policy(behavior, self.allow_movement)
        if behavior.movement != requested_movement:
            turn.warnings.append(f"movement {requested_movement} dropped: movement is disabled")
        requested_animation = behavior.animation
        behavior = apply_animation_policy(behavior, user_text if from_speech else None)
        if behavior.animation != requested_animation:
            turn.warnings.append(f"animation {requested_animation} dropped: no explicit dance order")
        turn.behavior = behavior
        if "fallback" not in " ".join(turn.warnings):
            self.history.add(session, user_text, behavior)

        t = time.perf_counter()
        try:
            turn.audio = self.tts.synthesize(behavior.text)
            turn.audio_id = self.audio_store.put(turn.audio)
        except TTSError as exc:
            logger.warning("TTS failure, replying without audio: %s", exc)
            turn.warnings.append(f"tts: {exc}")
        turn.timings_ms["tts"] = _ms(t)

        turn.timings_ms["total"] = _ms(t0)
        logger.info(
            "turn stt=%sms llm=%sms tts=%sms total=%sms tokens=%s | %r -> [%s/%s/%s/%s] %r",
            turn.timings_ms.get("stt", "-"),
            turn.timings_ms["llm"],
            turn.timings_ms["tts"],
            turn.timings_ms["total"],
            getattr(self.llm, "last_usage", None) or "-",
            user_text[:60],
            behavior.face,
            behavior.movement,
            behavior.neck,
            behavior.animation,
            behavior.text[:60],
        )
        return turn


def _ms(since: float) -> int:
    return int((time.perf_counter() - since) * 1000)
