#ifndef SENSORS_H
#define SENSORS_H

void sensorsInit();

float sensorsReadDistanceCm();     // wraps chassisReadDistanceCm()
void sensorsReadLine(char out[4]); // fills out[0..2] with '0'/'1' (L,C,R), out[3] = '\0'

#endif
