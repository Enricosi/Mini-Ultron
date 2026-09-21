#include <Arduino.h>
#include "command_parser.h"
#include "motor_controller.h"
#include "safety_controller.h"
#include "sensors.h"
#include "neck_controller.h"
#include "led_controller.h"

static void handleLine(const String &line, Stream &port) {
  if (line.length() == 0) return;

  if (line == "STOP") {
    stopCar();
    safetyNoteCommandReceived();
    port.println("OK");
    return;
  }

  if (line == "STATUS") {
    safetyNoteCommandReceived();
    port.print("DIST:");
    port.println(sensorsReadDistanceCm(), 1);
    char lineSensors[4];
    sensorsReadLine(lineSensors);
    port.print("LINE:");
    port.println(lineSensors);
    return;
  }

  if (line == "PING") {
    safetyNoteCommandReceived();
    port.println("OK");
    return;
  }

  int firstColon = line.indexOf(':');
  if (firstColon < 0) {
    port.println("ERR:PARSE");
    return;
  }
  String verb = line.substring(0, firstColon);
  String rest = line.substring(firstColon + 1);

  if (verb == "LED") {
    if (rest == "ON" || rest == "OFF") {
      safetyNoteCommandReceived();
      ledSetState(rest == "ON" ? LED_IDLE : LED_OFF);
      port.println("OK");
      return;
    }
    if (rest.startsWith("FACE:")) {
      LedState state;
      if (ledStateForFaceName(rest.substring(5), &state)) {
        safetyNoteCommandReceived();
        ledSetState(state);
        port.println("OK");
        return;
      }
    }
    port.println("ERR:PARSE");
    return;
  }

  if (verb == "NECK") {
    bool recognized = (rest == "CENTER" || rest == "LEFT" || rest == "RIGHT");
    if (!recognized) {
      port.println("ERR:PARSE");
      return;
    }
    safetyNoteCommandReceived();
    if (rest == "CENTER") {
      neckSetPosition(NECK_CENTER);
    } else if (rest == "LEFT") {
      neckSetPosition(NECK_LEFT);
    } else {
      neckSetPosition(NECK_RIGHT);
    }
    port.println("OK");
    return;
  }

  int secondColon = rest.indexOf(':');
  if (secondColon < 0) {
    port.println("ERR:PARSE");
    return;
  }
  String dir = rest.substring(0, secondColon);
  int speed = rest.substring(secondColon + 1).toInt();

  if (verb == "MOVE") {
    bool recognized = (dir == "FWD" || dir == "BACK" || dir == "LEFT" || dir == "RIGHT");
    if (!recognized) {
      port.println("ERR:PARSE");
      return;
    }
    safetyNoteCommandReceived();
    if (dir == "FWD") {
      float distanceCm = sensorsReadDistanceCm();
      if (safetyForwardBlocked(distanceCm)) {
        port.println("OBSTACLE");
        return;
      }
      moveForward(speed);
    } else if (dir == "BACK") {
      moveBackward(speed);
    } else if (dir == "LEFT") {
      strafeLeft(speed);
    } else if (dir == "RIGHT") {
      strafeRight(speed);
    }
    port.println("OK");
    return;
  }

  if (verb == "ROTATE") {
    bool recognized = (dir == "L" || dir == "R");
    if (!recognized) {
      port.println("ERR:PARSE");
      return;
    }
    safetyNoteCommandReceived();
    if (dir == "L") {
      rotateLeft(speed);
    } else {
      rotateRight(speed);
    }
    port.println("OK");
    return;
  }

  port.println("ERR:PARSE");
}

void commandParserInit(CommandParserState &state) {
  state.len = 0;
}

void commandParserPoll(Stream &port, CommandParserState &state) {
  while (port.available() > 0) {
    char c = port.read();
    if (c == '\n') {
      state.buffer[state.len] = '\0';
      handleLine(String(state.buffer), port);
      state.len = 0;
    } else if (c != '\r') {
      if (state.len < sizeof(state.buffer) - 1) {
        state.buffer[state.len++] = c;
      }
      // line too long for the buffer: extra chars are dropped until the
      // next newline, which will then parse as ERR:PARSE - safe, no move.
    }
  }
}
