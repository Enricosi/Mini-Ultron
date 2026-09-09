#include <Arduino.h>
#include "Freenove_4WD_Car_For_Pico_W.h"
#include "sensors.h"

void sensorsInit() {
  Ultrasonic_Setup();
  Track_Setup();
}

float sensorsReadDistanceCm() {
  return Get_Sonar();
}

void sensorsReadLine(char out[4]) {
  Track_Read();
  out[0] = '0' + sensorValue[0];
  out[1] = '0' + sensorValue[1];
  out[2] = '0' + sensorValue[2];
  out[3] = '\0';
}
