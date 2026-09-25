"""
Personality registry. Pipeline never hardcodes a character - it takes a Personality
object (system prompt + fallback line) and speaks in that voice.

To add a new personality: write a module here like ultron.py (a SYSTEM_PROMPT string
plus FALLBACK_TEXT/FALLBACK_FACE) and register one PERSONALITY entry below. Selected by
the ULTRON_PERSONALITY setting (services.py); "ultron" is the only one that exists
today, more are planned (per Enrico, another day).
"""
from dataclasses import dataclass


@dataclass(frozen=True)
class Personality:
    name: str
    system_prompt: str
    fallback_text: str   # what we say when the LLM is unreachable or returns garbage
    fallback_face: str = "GLITCH"


from . import ultron as _ultron  # noqa: E402  (needs Personality defined first)

PERSONALITIES: dict[str, Personality] = {
    _ultron.PERSONALITY.name: _ultron.PERSONALITY,
}

DEFAULT_PERSONALITY = _ultron.PERSONALITY


def get_personality(name: str) -> Personality:
    try:
        return PERSONALITIES[name]
    except KeyError:
        raise KeyError(f"unknown personality {name!r}, available: {sorted(PERSONALITIES)}") from None
