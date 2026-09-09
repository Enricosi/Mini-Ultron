/**********************************************************************
  Mini-Ultron - ESP32-S3 interaction controller.

  Owns the OLED face (face.h), the deterministic link to the Pico W
  locomotion/neck/safety controller (pico_link.h), and a development-
  only debug console (debug_console.h). Arm/PCA9685 gestures are
  preserved but deferred from v1 (arm_gestures.h).

  Type HELP over USB Serial (115200 baud) for the debug command list.
**********************************************************************/
#include "face.h"
#include "pico_link.h"
#include "arm_gestures.h"
#include "debug_console.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Mini-Ultron online");

    faceInit();
    picoLinkInit();
    debugConsoleInit();

#if MINI_ULTRON_ENABLE_ARMS
    armsInit();
#endif

    Serial.println("Type HELP for debug commands.");
}

void loop() {
    unsigned long now = millis();

    debugConsolePoll();
    picoLinkPoll();
    faceUpdate(now);
}
