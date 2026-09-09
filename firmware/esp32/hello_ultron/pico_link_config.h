/**********************************************************************
  Pico UART transport configuration.

  The permanent link to the Pico W is UART. The Pico side is fixed by
  hardware: GPIO0 = TX, GPIO1 = RX (see docs/CURRENT_STATE.md). The
  ESP32-S3 side RX/TX GPIOs have NOT been chosen yet - physical mounting
  needs to be far enough along to know what is accessible before picking
  them. Do NOT invent pin numbers here.

  Until real pins are chosen, PICO_LINK_USE_HARDWARE_UART stays undefined
  and pico_link.cpp runs in "disconnected" mode: every high-level pico*()
  call still validates its inputs and behaves the same way, but instead of
  transmitting it logs what it would have sent to the debug Serial console.
  This lets the rest of the firmware (debug console, future behavior code)
  be written and compiled today against the final API.

  Once ESP32 UART pins are chosen:
    1. Uncomment PICO_LINK_USE_HARDWARE_UART below.
    2. Set PICO_LINK_RX_PIN / PICO_LINK_TX_PIN to the real GPIO numbers.
    3. Wire Pico TX(GPIO0) -> ESP32 RX pin, Pico RX(GPIO1) <- ESP32 TX pin,
       and tie the two boards' grounds together.
  No other code needs to change.
**********************************************************************/
#ifndef PICO_LINK_CONFIG_H
#define PICO_LINK_CONFIG_H

// #define PICO_LINK_USE_HARDWARE_UART

#define PICO_LINK_RX_PIN  -1    // TBD - do not assign until wiring is decided
#define PICO_LINK_TX_PIN  -1    // TBD - do not assign until wiring is decided
#define PICO_LINK_BAUD    9600  // must match the Pico's Serial.begin(9600)

#endif
