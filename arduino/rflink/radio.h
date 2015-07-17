#ifndef RADIO_H
#define RADIO_H

#include "bareRFM69_const.h"

void radio_write_reg(uint8_t reg, uint8_t data);
uint8_t radio_read_reg(uint8_t reg);
void radio_write_fifo(uint8_t *data, int len);
void radio_read_fifo(uint8_t *data, int len);

bool radio_init(uint8_t node_id);

bool radio_rf_init(void);
void radio_send_packet(uint8_t len, uint8_t *data);

#endif /* RADIO_H */
