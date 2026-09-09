/**********************************************************************
  Vendored from Freenove's official "Freenove 4WD Car for Raspberry Pi
  Pico (W)" sketches (see ../../../freenove-car-reference/Mecanum_wheels).
  Trimmed to the sections Mini-Ultron's chassis actually uses:
    - Servo  (source: 01.2_Servo) - front/neck servo, wired to the shield
    - Motor  (source: 01.1_Car_Move_and_Turn)
    - Ultrasonic (source: 02.1_Ultrasonic_Ranging)
    - Track  (source: 04.2_Track_Car)
  Buzzer / battery / LED matrix / WS2812 / IR / photosensitive / emotion
  board sections were left out (not present on Mini-Ultron's cart). The
  Servo section only vendors Servo_Setup()/Servo_1_Angle() - the official
  offset/sweep helpers weren't needed for a 3-position deterministic neck
  and were left out to avoid unused delay()-based code in this layer.
  Function bodies are copied verbatim from the official sketches - do
  not edit them here. If Mini-Ultron needs different behavior, add it
  in motor_controller / safety_controller / sensors / neck_controller
  instead.

  Requires the third-party "RP2040_PWM" library (khoih-prog) installed
  via the Arduino Library Manager - it is not bundled with this repo.
**********************************************************************/
#ifndef _FREENOVE_4WD_CAR_H
#define _FREENOVE_4WD_CAR_H

#include <Arduino.h>
#include "RP2040_PWM.h"

// #define REVERSE_MOTOR1
// #define REVERSE_MOTOR2
// #define REVERSE_MOTOR3
// #define REVERSE_MOTOR4

/////////////////////Servo drive area///////////////////////////////////
#define PIN_SERVO1  13    //define servo pin (front/neck servo, shield header)

void Servo_Setup(void);           //servo initialization
void Servo_1_Angle(float angle);  //Set the rotation parameters of servo 1, and the parameters are 30-150 degrees

/////////////////////Motor drive area///////////////////////////////////
#define PIN_MOTOR_PWM_RIGHT1  7          //Define the positive pole of M3
#define PIN_MOTOR_PWM_RIGHT2  6          //Define the positive pole of M3
#define PIN_MOTOR_PWM_RIGHT3  9          //Define the negative pole of M4
#define PIN_MOTOR_PWM_RIGHT4  8          //Define the negative pole of M4
#define PIN_MOTOR_PWM_LEFT1   18         //Define the positive pole of M1
#define PIN_MOTOR_PWM_LEFT2   19         //Define the negative pole of M1
#define PIN_MOTOR_PWM_LEFT3   21         //Define the positive pole of M2
#define PIN_MOTOR_PWM_LEFT4   20         //Define the negative pole of M2
#define MOTOR_SPEED_MIN       -100       //Define a minimum speed limit for wheels
#define MOTOR_SPEED_MAX       100        //Define a maximum speed limit for wheels

void Motor_Setup(void);                //motor initialization
void Motor_Move_Init(int m1_speed, int m2_speed, int m3_speed, int m4_speed);//A function to control the car motor
void Motor_Move(int Left_speed, int Right_speed);//A function to control the car motor
void Motor_M_Move(int M1_speed, int M2_speed, int M3_speed, int M4_speed);//A function to control the maicanum wheel car motor

/////////////////////Ultrasonic drive area/////////////////////////////////
#define PIN_SONIC_TRIG    4             //define Trig pin
#define PIN_SONIC_ECHO    5             //define Echo pin
#define MAX_DISTANCE      300           //cm
#define SONIC_TIMEOUT (MAX_DISTANCE*60) // calculate timeout
#define SOUND_VELOCITY    340           //soundVelocity: 340m/s
void Ultrasonic_Setup(void);//Ultrasonic initialization
float Get_Sonar(void);//Obtain ultrasonic distance data

/////////////////////Track drive area//////////////////////////////
#define PIN_TRACKING_LEFT   12
#define PIN_TRACKING_CENTER 11
#define PIN_TRACKING_RIGHT  10
extern unsigned char sensorValue[4];
void Track_Setup(void);//Trace module initialization
void Track_Read(void);//Tracking module reading

#endif
