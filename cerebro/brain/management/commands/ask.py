"""Talk to Mini-Ultron from the terminal (text in, behavior out, optional WAV).
Handy for testing the LLM + personality without any hardware."""
from django.core.management.base import BaseCommand

from brain import audio_utils, services


class Command(BaseCommand):
    help = "Send text through LLM -> validation -> TTS and print the behavior."

    def add_arguments(self, parser):
        parser.add_argument("text", nargs="?", help="what to say; omit for an interactive chat")
        parser.add_argument("--wav", help="write the last reply's audio to this WAV file")

    def handle(self, *args, **options):
        pipeline = services.get_pipeline()
        prompts = [options["text"]] if options["text"] else None
        while True:
            text = prompts.pop(0) if prompts else (None if options["text"] else input("tú> ").strip())
            if not text:
                break
            turn = pipeline.handle_text(text, "cli")
            if turn.ignored:
                self.stdout.write(f"(ignored: {turn.ignored_reason})")
            else:
                b = turn.behavior
                self.stdout.write(f"ultron> {b.text}\n   face={b.face} movement={b.movement} neck={b.neck}")
                self.stdout.write(f"   timings_ms={turn.timings_ms} warnings={turn.warnings}")
                if options["wav"] and turn.audio:
                    with open(options["wav"], "wb") as f:
                        f.write(audio_utils.wav_bytes(turn.audio.pcm))
            if options["text"]:
                break
