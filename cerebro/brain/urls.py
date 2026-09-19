from django.urls import path

from . import views

urlpatterns = [
    path("converse", views.converse),
    path("converse_text", views.converse_text),
    path("audio/<str:audio_id>", views.audio),
    path("health", views.health),
]
