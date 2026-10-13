"""
HTTP API used by the ESP32 (and by curl for debugging).

  POST /api/converse       body = utterance audio (raw PCM16LE mono 16 kHz, or a WAV)
                           header X-Session-Id (optional)
                           -> JSON: transcript, behavior, audio{url,...}, timings_ms
  GET  /api/audio/<id>     the reply audio: raw PCM16LE mono 16 kHz (Content-Length set)
  POST /api/converse_text  {"text": "...", "session": "..."} -> same JSON (no STT; for testing)
  POST /api/event          {"event": "JOLT", "session": "..."} -> same JSON (no STT/utterance;
                           a controller-reported physical event, see pipeline.EVENT_PROMPTS)
  GET  /api/health         which backends are configured (never returns secrets)
"""
import json
import logging

from django.conf import settings
from django.http import HttpResponse, JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_GET, require_POST

from . import audio_utils, services

logger = logging.getLogger("brain.views")

SAMPLE_FORMAT = "pcm_s16le_mono"


def _session(request) -> str:
    return request.headers.get("X-Session-Id", "default")[:64]


def _turn_json(turn) -> dict:
    data = {
        "ok": True,
        "ignored": turn.ignored,
        "transcript": turn.transcript,
        "timings_ms": turn.timings_ms,
        "warnings": turn.warnings,
    }
    if turn.ignored:
        data["reason"] = turn.ignored_reason
        return data
    data["behavior"] = turn.behavior.to_dict()
    if turn.audio is not None:
        data["audio"] = {
            "url": f"/api/audio/{turn.audio_id}",
            "bytes": len(turn.audio.pcm),
            "sample_rate": turn.audio.sample_rate,
            "format": SAMPLE_FORMAT,
        }
    else:
        data["audio"] = None
    return data


def _save_debug_audio(body: bytes) -> None:
    if not settings.ULTRON_SAVE_DEBUG_AUDIO:
        return
    try:
        debug_dir = settings.ULTRON_DATA_DIR / "debug"
        debug_dir.mkdir(parents=True, exist_ok=True)
        pcm = body if body[:4] != b"RIFF" else audio_utils.float_to_pcm16(audio_utils.decode_upload(body))
        (debug_dir / "last_utterance.wav").write_bytes(audio_utils.wav_bytes(pcm))
    except Exception:  # debugging aid only, never break a turn
        logger.exception("could not save debug audio")


@csrf_exempt
@require_POST
def converse(request):
    body = request.body
    if len(body) > settings.ULTRON_MAX_BODY_BYTES:
        return JsonResponse({"ok": False, "error": "body_too_large"}, status=413)
    _save_debug_audio(body)
    try:
        turn = services.get_pipeline().handle_audio(body, _session(request))
    except Exception:
        logger.exception("converse failed")
        return JsonResponse({"ok": False, "error": "internal_error"}, status=500)
    return JsonResponse(_turn_json(turn))


def _json_turn(request, field: str, handler_name: str, normalize=str):
    """Shared body of the JSON endpoints: parse {field, session}, run the pipeline, answer."""
    try:
        payload = json.loads(request.body or b"{}")
        value = normalize(str(payload.get(field, "")))
        session = str(payload.get("session", "default"))[:64]
    except (json.JSONDecodeError, AttributeError):
        return JsonResponse({"ok": False, "error": "bad_json"}, status=400)
    try:
        turn = getattr(services.get_pipeline(), handler_name)(value, session)
    except Exception:
        logger.exception("%s failed", handler_name)
        return JsonResponse({"ok": False, "error": "internal_error"}, status=500)
    return JsonResponse(_turn_json(turn))


@csrf_exempt
@require_POST
def converse_text(request):
    return _json_turn(request, "text", "handle_text")


@csrf_exempt
@require_POST
def event(request):
    return _json_turn(request, "event", "handle_event", normalize=lambda v: v.strip().upper())


@require_GET
def audio(request, audio_id: str):
    speech = services.get_pipeline().audio_store.get(audio_id)
    if speech is None:
        return JsonResponse({"ok": False, "error": "not_found"}, status=404)
    response = HttpResponse(speech.pcm, content_type="application/octet-stream")
    response["X-Sample-Rate"] = str(speech.sample_rate)
    response["X-Sample-Format"] = SAMPLE_FORMAT
    return response


@require_GET
def health(request):
    pipeline = services.get_pipeline()
    return JsonResponse(
        {
            "ok": True,
            "llm": {
                "provider": pipeline.llm.name,
                "model": pipeline.llm.model,
                "key_configured": getattr(pipeline.llm, "key_configured", True),
                "fallback": pipeline.fallback_llm.label if pipeline.fallback_llm else None,
            },
            "stt": {"backend": pipeline.stt.name, "model": getattr(pipeline.stt, "model_name", "")},
            "tts": {"backend": pipeline.tts.name, "voice": getattr(pipeline.tts, "voice_name", "")},
            "movement_allowed": pipeline.allow_movement,
            "personality": pipeline.personality.name,
        }
    )
