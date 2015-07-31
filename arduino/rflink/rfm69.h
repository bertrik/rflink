#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>
#include <stdbool.h>

void radio_write_reg(uint8_t reg, uint8_t data);
uint8_t radio_read_reg(uint8_t reg);

bool radio_init(uint8_t node_id);
int radio_set_power(int dbm);
void radio_set_frequency(uint32_t khz);

void radio_send_packet(uint8_t len, uint8_t *data);

bool radio_packet_avail(void);
bool radio_recv_packet(uint8_t *len_p, uint8_t *data, int size);


#endif /* RADIO_H */
