"""
The whole CEREBRO turn: audio -> STT -> LLM -> validated Behavior -> TTS.

Failure policy (the robot should never just go silent):
- STT returns nothing / a Whisper hallucination -> the turn is "ignored", no reply.
- Main LLM fails or returns unusable output     -> the one configured fallback LLM, if any
                                                   (warning "llm_fallback: ..."; never silent).
- Fallback also fails / none configured         -> canned line + GLITCH face (turn.degraded).
- TTS fails                                     -> behavior is still returned, without audio.
"""
import json
import logging
import threading
import time
import uuid
from collections import OrderedDict, deque
from dataclasses import dataclass, field

from . import audio_utils
from .llm import LLMBackend, LLMError
from .personalities import DEFAULT_PERSONALITY, Personality
from .schemas import Behavior, BehaviorError, apply_animation_policy, apply_policy, parse_behavior
from .stt import STTBackend, is_hallucination
from .tts import Speech, TTSBackend, TTSError

logger = logging.getLogger("brain.pipeline")

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
    """Rolling short-term memory per session, in RAM (no persistence). The number of
    sessions is bounded too: X-Session-Id is client-supplied, so old ones are dropped."""

    def __init__(self, max_turns: int = 6, max_sessions: int = 32):
        self.max_turns = max_turns
        self.max_sessions = max_sessions
        self._turns: OrderedDict[str, deque] = OrderedDict()
        self._lock = threading.Lock()

    def messages(self, session: str) -> list[dict]:
        with self._lock:
            turns = list(self._turns.get(session, ()))
        out: list[dict] = []
        for user_text, behavior_json in turns:
            out.append({"role": "user", "content": user_text})
            out.append({"role": "assistant", "content": behavior_json})
        return out

    def add(self, session: str, user_text: str, behavior: Behavior) -> None:
        with self._lock:
            turns = self._turns.setdefault(session, deque(maxlen=self.max_turns))
            self._turns.move_to_end(session)
            turns.append((user_text, json.dumps(behavior.to_dict(), ensure_ascii=False)))
            while len(self._turns) > self.max_sessions:
                self._turns.popitem(last=False)


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
    id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])  # interaction id, in every log line
    llm_used: str = ""  # "provider/model" that produced the reply; "" = canned line
    degraded: bool = False  # True = no LLM answered, the canned line was used
    usage: dict = field(default_factory=dict)  # token counts of the LLM call that answered


class Pipeline:
    def __init__(
        self,
        stt: STTBackend,
        llm: LLMBackend,
        tts: TTSBackend,
        fallback_llm: LLMBackend | None = None,
        history: History | None = None,
        audio_store: AudioStore | None = None,
        allow_movement: bool = False,
        max_reply_chars: int = 280,
        personality: Personality = DEFAULT_PERSONALITY,
        min_audio_s: float = 0.3,
    ):
        self.stt = stt
        self.llm = llm
        self.fallback_llm = fallback_llm
        self.min_audio_s = min_audio_s
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
        if len(audio) / audio_utils.SAMPLE_RATE < self.min_audio_s:
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
        logger.info("[%s] turn ignored (%s) stt=%sms", turn.id, reason, turn.timings_ms.get("stt", "-"))
        return turn

    def _ask_llm(self, turn: Turn, messages: list[dict]) -> Behavior:
        """Main LLM, then the configured fallback, then the canned line. Every skip is logged."""
        for llm in (self.llm, self.fallback_llm):
            if llm is None:
                continue
            t = time.perf_counter()
            try:
                behavior, warnings = parse_behavior(llm.complete(messages), self.max_reply_chars)
            except (LLMError, BehaviorError) as exc:
                kind = getattr(exc, "kind", "invalid_response")
                logger.warning("[%s] %s failed (%s, %dms): %s", turn.id, llm.label, kind, _ms(t), exc)
                turn.warnings.append(f"llm_error: {llm.label} {kind}: {exc}")
                continue
            turn.warnings += warnings
            turn.llm_used = llm.label
            turn.usage = dict(llm.last_usage or {})
            return behavior
        turn.degraded = True
        logger.error("[%s] no LLM answered, using the canned line", turn.id)
        return Behavior(text=self.personality.fallback_text, face=self.personality.fallback_face)

    def _respond(self, turn: Turn, user_text: str, session: str, t0: float, from_speech: bool = True) -> Turn:
        messages = [{"role": "system", "content": self.personality.system_prompt}]
        messages += self.history.messages(session)
        messages.append({"role": "user", "content": user_text})

        t = time.perf_counter()
        behavior = self._ask_llm(turn, messages)
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
        if not turn.degraded:
            self.history.add(session, user_text, behavior)

        t = time.perf_counter()
        try:
            turn.audio = self.tts.synthesize(behavior.text)
            turn.audio_id = self.audio_store.put(turn.audio)
        except TTSError as exc:
            logger.warning("[%s] TTS failure, replying without audio: %s", turn.id, exc)
            turn.warnings.append(f"tts: {exc}")
        turn.timings_ms["tts"] = _ms(t)

        turn.timings_ms["total"] = _ms(t0)
        logger.info(
            "[%s] turn llm=%s stt=%sms llm=%sms tts=%sms total=%sms tokens=%s | %r -> [%s/%s/%s/%s] %r",
            turn.id,
            turn.llm_used or "canned",
            turn.timings_ms.get("stt", "-"),
            turn.timings_ms["llm"],
            turn.timings_ms["tts"],
            turn.timings_ms["total"],
            turn.usage or "-",
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
