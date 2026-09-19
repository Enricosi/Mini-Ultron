/**********************************************************************
  Wi-Fi + CEREBRO server settings. THIS FILE IS A TEMPLATE.

  Copy it to wifi_config.h (same folder; gitignored, so your Wi-Fi password
  never enters Git) and fill in your values. Without wifi_config.h the
  firmware still builds and runs exactly as before, with the brain link
  disabled (see brain_config.h).
**********************************************************************/
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#define WIFI_SSID      "your-wifi-name"
#define WIFI_PASSWORD  "your-wifi-password"

// The home-lab mini server that runs CEREBRO (Django). Use a fixed IP or a hostname
// the ESP32 can resolve; give the server a static/reserved address on your router.
#define CEREBRO_HOST   "192.168.1.50"
#define CEREBRO_PORT   8000

// Let CEREBRO's abstract movement intents (FORWARD, ROTATE_LEFT, ...) actually drive
// the robot. OFF by default. Even when on: fixed slow speed, ~0.8 s bursts, the Pico
// keeps its watchdog / obstacle veto / STOP authority, and CEREBRO must also allow it
// (ULTRON_ALLOW_MOVEMENT=1 on the server). Only enable with the robot safely on the floor.
// #define MINI_ULTRON_BRAIN_MOVEMENT 1

#endif
