import os

from django.core.wsgi import get_wsgi_application

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
application = get_wsgi_application()

from django.conf import settings  # noqa: E402

from brain import services  # noqa: E402

services.get_pipeline()  # validate the config now: a bad .env must stop the server at start
if settings.ULTRON_PRELOAD:
    services.preload()  # load Whisper/Piper now so the first request isn't slow
