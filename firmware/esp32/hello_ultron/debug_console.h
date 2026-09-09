/**********************************************************************
  Development-only line-based debug console over USB Serial (115200
  baud, newline-terminated). Every command is a full, explicit keyword -
  there is no single-key shortcut that could fire a movement command by
  accident, and any unrecognized/malformed line is rejected with
  ERR:PARSE instead of guessing. See printHelp() in debug_console.cpp
  for the full command list, or type HELP over serial.
**********************************************************************/
#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

void debugConsoleInit();
void debugConsolePoll();  // call every loop() iteration - non-blocking

#endif
