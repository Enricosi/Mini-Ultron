/**********************************************************************
  Mini-Ultron - Pico W locomotion firmware.

  Chassis: Freenove 4WD Car Kit (Mecanum) for Raspberry Pi Pico W.
  Role: last-mile motor control + local safety. Takes high-level move
  commands over Serial (USB today, Serial1/UART from the ESP32-S3 once
  that link is wired up) and never trusts a command blindly - see
  safety_controller.h for the rules.

  Test today from the Arduino Serial Monitor (9600 baud, newline
  ending) with lines like:
    MOVE:FWD:40
    ROTATE:L:25
    NECK:LEFT
    STOP
    STATUS
**********************************************************************/
#include "Freenove_4WD_Car_For_Pico_W.h"
#include "motor_controller.h"
#include "safety_controller.h"
#include "sensors.h"
#include "neck_controller.h"
#include "command_parser.h"

static CommandParserState usbParserState;
static CommandParserState uartParserState;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // UART to the ESP32-S3 (GPIO0 TX / GPIO1 RX)

  Motor_Setup();
  motorControllerInit();  // motors explicitly stopped before anything else
  neckControllerInit();   // neck moves to its safe, centered startup position
  sensorsInit();
  safetyInit();
  commandParserInit(usbParserState);
  commandParserInit(uartParserState);

  Serial.println("READY");
}

void loop() {
  commandParserPoll(Serial, usbParserState);
  commandParserPoll(Serial1, uartParserState);

  if (safetyCheckTimeout()) {
    Serial.println("STOPPED:TIMEOUT");
    Serial1.println("STOPPED:TIMEOUT");
  }
}
