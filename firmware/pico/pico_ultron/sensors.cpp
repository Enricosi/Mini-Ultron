#include <Arduino.h>
#include "chassis_hal.h"
#include "sensors.h"

void sensorsInit() {
  chassisUltrasonicInit();
  chassisTrackInit();
}

float sensorsReadDistanceCm() {
  return chassisReadDistanceCm();
}

void sensorsReadLine(char out[4]) {
  bool line[3];
  chassisReadTrack(line);
  out[0] = '0' + line[0];
  out[1] = '0' + line[1];
  out[2] = '0' + line[2];
  out[3] = '\0';
}
