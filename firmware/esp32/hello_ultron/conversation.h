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
**********************************************************************/
#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <Arduino.h>

void conversationInit();
void conversationUpdate(unsigned long now);  // call every loop() iteration - non-blocking

#endif
