/**********************************************************************
  ASCII line protocol, one command per line, fields separated by ':'.
  Works against any Stream, so the same code will later serve both
  Serial (USB, for bench testing) and Serial1 (UART to the ESP32-S3).

  Inbound:
    MOVE:FWD:<0-100>   MOVE:BACK:<0-100>
    MOVE:LEFT:<0-100>  MOVE:RIGHT:<0-100>   (strafe)
    ROTATE:L:<0-100>   ROTATE:R:<0-100>     (rotate in place)
    NECK:CENTER        NECK:LEFT           NECK:RIGHT
    LED:ON             LED:OFF             (car-shield WS2812 status LEDs)
    LED:FACE:<NAME>    same 9 names as the ESP32's FaceState (e.g. LED:FACE:ANGRY)
    STOP
    STATUS
    PING

  Outbound:
    OK                 command accepted (and executed, if it involved motion)
    ERR:PARSE          line didn't match any known command shape
    OBSTACLE           MOVE:FWD refused - ultrasonic sees something too close
    DIST:<cm>          part of the STATUS reply
    LINE:<3 bits>       part of the STATUS reply, e.g. LINE:010 (L,C,R)
**********************************************************************/
#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

struct CommandParserState {
  char buffer[64];
  uint8_t len = 0;
};

void commandParserInit(CommandParserState &state);
void commandParserPoll(Stream &port, CommandParserState &state);

#endif
