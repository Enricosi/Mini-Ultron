/**********************************************************************
  Pico UART transport configuration.

  The permanent link to the Pico W is UART. The Pico side is fixed by
  hardware: GPIO0 = TX, GPIO1 = RX (see docs/CURRENT_STATE.md).

  2026-09-15: ESP32-S3 side pins chosen and physically wired by Enrico:
  ESP32 GPIO17 = RX (from Pico GPIO0/TX), ESP32 GPIO18 = TX (to Pico
  GPIO1/RX), grounds tied together. No 5V/3.3V shared between boards.
  Confirmed free of conflict with the OLED's software-SPI pins
  (GPIO47/21/2/41/42).

  Once ESP32 UART pins are chosen:
    1. Uncomment PICO_LINK_USE_HARDWARE_UART below.
    2. Set PICO_LINK_RX_PIN / PICO_LINK_TX_PIN to the real GPIO numbers.
    3. Wire Pico TX(GPIO0) -> ESP32 RX pin, Pico RX(GPIO1) <- ESP32 TX pin,
       and tie the two boards' grounds together.
  No other code needs to change.
**********************************************************************/
#ifndef PICO_LINK_CONFIG_H
#define PICO_LINK_CONFIG_H

#define PICO_LINK_USE_HARDWARE_UART

#define PICO_LINK_RX_PIN  17    // ESP32 RX <- Pico GPIO0/TX
#define PICO_LINK_TX_PIN  18    // ESP32 TX -> Pico GPIO1/RX
#define PICO_LINK_BAUD    9600  // must match the Pico's Serial.begin(9600)

#endif
