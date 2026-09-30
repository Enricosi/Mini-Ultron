"""
The ONLY shape the LLM may produce, and the validation that turns whatever it
returned into a safe, bounded Behavior.

The vocabulary mirrors what the firmware actually implements (face.h FaceState,
pico protocol MOVE/ROTATE/NECK). There are no numeric/hardware fields here on
purpose: no PWM, no angles, no speeds, no durations. The ESP32 decides those.
LISTENING and THINKING faces are deliberately absent: they are system states the
ESP32 sets itself, not something the LLM picks.
"""
import json
import re
from dataclasses import asdict, dataclass, replace

FACES = ("IDLE", "ANGRY", "SURPRISED", "SUSPICIOUS", "SPEAKING", "GLITCH", "SLEEPING")
MOVEMENTS = (
    "NONE",
    "FORWARD",
    "BACKWARD",
    "STRAFE_LEFT",
    "STRAFE_RIGHT",
    "ROTATE_LEFT",
    "ROTATE_RIGHT",
)
NECKS = ("NONE", "CENTER", "LEFT", "RIGHT")
# Custom face animations built into the ESP32 firmware (anim_library.cpp). Each name
# here must exist there; the firmware lowercases it and ignores unknown names.
ANIMATIONS = ("NONE", "CALABAZA")

DEFAULT_FACE = "SPEAKING"  # the plain "talking" face
DEFAULT_MOVEMENT = "NONE"
DEFAULT_NECK = "NONE"
DEFAULT_ANIMATION = "NONE"

# An animation only plays when the user literally asked for a dance in this very
# utterance - the LLM choosing one is not enough (see apply_animation_policy).
DANCE_ORDER_RE = re.compile(r"\bbail|\bdanz|calabaza", re.IGNORECASE)

# Strict-mode friendly (all fields required, no extra properties).
BEHAVIOR_JSON_SCHEMA = {
    "type": "object",
    "properties": {
        "text": {"type": "string"},
        "face": {"type": "string", "enum": list(FACES)},
        "movement": {"type": "string", "enum": list(MOVEMENTS)},
        "neck": {"type": "string", "enum": list(NECKS)},
        "animation": {"type": "string", "enum": list(ANIMATIONS)},
    },
    "required": ["text", "face", "movement", "neck", "animation"],
    "additionalProperties": False,
}


class BehaviorError(ValueError):
    """The LLM output could not be turned into a usable Behavior."""


@dataclass(frozen=True)
class Behavior:
    text: str
    face: str = DEFAULT_FACE
    movement: str = DEFAULT_MOVEMENT
    neck: str = DEFAULT_NECK
    animation: str = DEFAULT_ANIMATION

    def to_dict(self) -> dict:
        return asdict(self)


def extract_json(raw: str) -> dict:
    """Parse the model output as a JSON object, tolerating code fences / chatter."""
    if not isinstance(raw, str) or not raw.strip():
        raise BehaviorError("empty LLM output")
    text = raw.strip()
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        text = re.sub(r"^```(?:json)?\s*|\s*```$", "", text, flags=re.IGNORECASE)
        start, end = text.find("{"), text.rfind("}")
        if start == -1 or end <= start:
            raise BehaviorError("no JSON object in LLM output") from None
        try:
            data = json.loads(text[start : end + 1])
        except json.JSONDecodeError as exc:
            raise BehaviorError(f"invalid JSON in LLM output: {exc}") from None
    if not isinstance(data, dict):
        raise BehaviorError("LLM output JSON is not an object")
    return data


def clean_text(text: str, max_chars: int) -> str:
    """Make text safe/pleasant to speak: no markdown, one line, bounded length."""
    text = re.sub(r"[*_`#>]+", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    if len(text) <= max_chars:
        return text
    cut = text[:max_chars]
    sentence_end = max(cut.rfind("."), cut.rfind("!"), cut.rfind("?"), cut.rfind("…"))
    if sentence_end >= max_chars // 2:
        return cut[: sentence_end + 1]
    return cut.rsplit(" ", 1)[0].rstrip(",;:") + "…"


def _coerce_enum(value, allowed, default, field, warnings) -> str:
    candidate = str(value).strip().upper() if value is not None else ""
    if candidate in allowed:
        return candidate
    if value is not None:
        warnings.append(f"invalid {field} {value!r} -> {default}")
    return default


def parse_behavior(raw: str, max_chars: int = 280) -> tuple[Behavior, list[str]]:
    """
    Turn raw LLM output into a Behavior. Unknown/missing face, movement, neck or animation are
    replaced with safe defaults (and reported in `warnings`); missing text is fatal.
    """
    data = extract_json(raw)
    warnings: list[str] = []

    text = data.get("text")
    if not isinstance(text, str) or not text.strip():
        raise BehaviorError("LLM output has no usable 'text'")
    text = clean_text(text, max_chars)
    if not text:
        raise BehaviorError("LLM 'text' is empty after cleaning")

    return (
        Behavior(
            text=text,
            face=_coerce_enum(data.get("face"), FACES, DEFAULT_FACE, "face", warnings),
            movement=_coerce_enum(
                data.get("movement"), MOVEMENTS, DEFAULT_MOVEMENT, "movement", warnings
            ),
            neck=_coerce_enum(data.get("neck"), NECKS, DEFAULT_NECK, "neck", warnings),
            animation=_coerce_enum(
                data.get("animation"), ANIMATIONS, DEFAULT_ANIMATION, "animation", warnings
            ),
        ),
        warnings,
    )


def apply_policy(behavior: Behavior, allow_movement: bool) -> Behavior:
    """Server-side gate applied after validation. Movement is dropped unless enabled."""
    if not allow_movement and behavior.movement != DEFAULT_MOVEMENT:
        return replace(behavior, movement=DEFAULT_MOVEMENT)
    return behavior


def apply_animation_policy(behavior: Behavior, user_speech: str | None) -> Behavior:
    """
    Dance only on an explicit order: the animation is dropped unless the user's own
    words (never an event prompt - pass None for those) contain a dance request.
    """
    if behavior.animation == DEFAULT_ANIMATION:
        return behavior
    if user_speech and DANCE_ORDER_RE.search(user_speech):
        return behavior
    return replace(behavior, animation=DEFAULT_ANIMATION)
