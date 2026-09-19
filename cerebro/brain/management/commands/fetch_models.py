"""Download the Piper voice and the faster-whisper model into ULTRON_DATA_DIR.
Run this once on the machine that will host CEREBRO (the home-lab server)."""
from django.conf import settings
from django.core.management.base import BaseCommand


class Command(BaseCommand):
    help = "Download the configured Piper voice and faster-whisper model."

    def handle(self, *args, **options):
        data_dir = settings.ULTRON_DATA_DIR
        voices_dir = data_dir / "voices"
        voices_dir.mkdir(parents=True, exist_ok=True)

        self.stdout.write(f"Piper voice {settings.TTS_VOICE} -> {voices_dir}")
        from piper.download_voices import download_voice

        download_voice(settings.TTS_VOICE, voices_dir)

        self.stdout.write(f"faster-whisper {settings.STT_MODEL} -> {data_dir / 'whisper'}")
        from faster_whisper import WhisperModel

        WhisperModel(
            settings.STT_MODEL,
            device="cpu",
            compute_type=settings.STT_COMPUTE_TYPE,
            download_root=str(data_dir / "whisper"),
        )
        self.stdout.write(self.style.SUCCESS("Models ready."))
