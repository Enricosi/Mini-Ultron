"""Run a WAV file through the FULL pipeline (STT -> LLM -> TTS), like the ESP32 would."""
from django.core.management.base import BaseCommand

from brain import audio_utils, services


class Command(BaseCommand):
    help = "Feed a WAV/raw-PCM file to the full pipeline and print the result."

    def add_arguments(self, parser):
        parser.add_argument("audio_file")
        parser.add_argument("--out", help="write the reply audio to this WAV file")

    def handle(self, *args, **options):
        body = open(options["audio_file"], "rb").read()
        turn = services.get_pipeline().handle_audio(body, "cli")
        self.stdout.write(f"transcript: {turn.transcript!r}")
        if turn.ignored:
            self.stdout.write(f"ignored: {turn.ignored_reason}")
        else:
            b = turn.behavior
            self.stdout.write(f"reply: {b.text}\nface={b.face} movement={b.movement} neck={b.neck}")
            if options["out"] and turn.audio:
                with open(options["out"], "wb") as f:
                    f.write(audio_utils.wav_bytes(turn.audio.pcm))
        self.stdout.write(f"timings_ms={turn.timings_ms} warnings={turn.warnings}")
