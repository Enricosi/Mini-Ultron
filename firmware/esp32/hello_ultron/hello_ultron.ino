/**********************************************************************
  Mini-Ultron - ESP32-S3 interaction controller.

  Owns the OLED face (face.h), the deterministic link to the Pico W
  locomotion/neck/safety controller (pico_link.h), mic + speaker audio
  with continuous voice detection (audio.h), custom face animations
  streamed from PROGMEM/LittleFS (anim_player.h), the Wi-Fi/HTTP link to the
  CEREBRO brain server (brain_link.h) and the conversation flow that ties
  them together (conversation.h), the IMU (imu.h), and a development-
  only debug console (debug_console.h). Arm/PCA9685 gestures are
  preserved but deferred from v1 (arm_gestures.h).

  Type HELP over USB Serial (115200 baud) for the debug command list.
**********************************************************************/
#include "face.h"
#include "anim_player.h"
#include "pico_link.h"
#include "audio.h"
#include "brain_link.h"
#include "conversation.h"
#include "imu.h"
#include "motion.h"
#include "arm_gestures.h"
#include "debug_console.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Mini-Ultron online");

    faceInit();
    animInit();
    picoLinkInit();
    audioInit();
    brainInit();
    conversationInit();
    imuInit();
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
    conversationUpdate(now);
    motionUpdate(now);
    imuUpdate(now);
    faceUpdate(now);
}
