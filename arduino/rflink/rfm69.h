#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>
#include <stdbool.h>

// low-level RFM69 read/write register
void radio_write_reg(uint8_t reg, uint8_t data);
uint8_t radio_read_reg(uint8_t reg);

// initialises the RFM69, using the specified address
bool radio_init(uint8_t node_id);

// sets radio power (in dBm units)
int radio_set_power(int dbm);
// sets carrier frequency (863-870 MHz)
uint32_t radio_set_frequency(uint32_t khz);

// sends a packet over the air
void radio_send_packet(uint8_t len, uint8_t *data);

// indicates if a packet was received
bool radio_packet_avail(void);
// reads the packet from the RFM69 FIFO
bool radio_recv_packet(uint8_t *len_p, uint8_t *data, int size);


#endif /* RADIO_H */
