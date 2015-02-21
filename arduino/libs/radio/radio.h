#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>

#include <SPI.h>
#include <RH_RF69.h>

// initializes the radio (frequency, modulation, power, etc.)
bool radio_init(RH_RF69 *rf69, uint8_t address);

#endif /* RADIO_H */
