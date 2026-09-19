/**********************************************************************
  Compile-time switches for the CEREBRO (Django brain) link.

  The Wi-Fi credentials are secrets, so they live in wifi_config.h, which is
  gitignored (template: wifi_config.example.h). When that file is absent the
  firmware compiles and behaves as before - voice detection and face reactions
  only - and the brain link just logs that it is disabled. Same
  "gate it, harmless fallback" pattern as pico_link_config.h.
**********************************************************************/
#ifndef BRAIN_CONFIG_H
#define BRAIN_CONFIG_H

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#define CEREBRO_CONFIGURED 1
#else
#define CEREBRO_CONFIGURED 0
#endif

// Movement from CEREBRO is opt-in (see wifi_config.example.h).
#ifndef MINI_ULTRON_BRAIN_MOVEMENT
#define MINI_ULTRON_BRAIN_MOVEMENT 0
#endif

#endif
