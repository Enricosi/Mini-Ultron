/**********************************************************************
  Conversation flow: ties the audio module, the CEREBRO link, the face, the
  neck and the movement executor into one small non-blocking state machine.

    IDLE -> (speech starts) LISTENING -> (speech ends) THINKING
         -> (reply arrives) SPEAKING -> (audio done) HOLD -> IDLE

  Faces: LISTENING while the user talks, THINKING while CEREBRO works, then the
  face CEREBRO chose (default SPEAKING) while the reply plays, held ~1.5 s after.
  The mic is deaf from the end of an utterance until HOLD ends, so the robot never
  hears itself and the capture buffer is safe.

  Without a configured brain link (no wifi_config.h) this degrades to the previous
  behavior: LISTENING while speech is detected, restoring the previous face after.
  Names coming from CEREBRO are matched against exact vocabularies here; anything
  unknown is ignored.

  A physical event (currently: an IMU jolt, see imu.h) can also start a turn instead
  of speech: conversationUpdate() watches imuGetData() and, when idle, reports a new
  jolt the same way debug_console's EVENT:JOLT does, via conversationTriggerEvent().
  It reuses THINKING/SPEAKING/HOLD as-is; CEREBRO decides what (if anything) to say.
**********************************************************************/
#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <Arduino.h>

void conversationInit();
void conversationUpdate(unsigned long now);  // call every loop() iteration - non-blocking

// Reports a physical event to CEREBRO as if it were a turn (see brain_link's
// EVENT_PROMPTS-backed vocabulary, currently just "JOLT"). False if a turn is already
// in progress or the brain link is busy/disabled: the caller should just drop it, the
// event was not "lost" in any safety sense, there is simply nothing to react to yet.
bool conversationTriggerEvent(const char *eventName);

#endif
