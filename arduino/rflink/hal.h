/*
 * Simple abstraction layer above Arduino hardware
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>

// SPI functions
void spi_init(void);
void spi_setup(int speed, int flags);
int spi_send(int num, char *buf);
int spi_recv(int num, char *buf);

// serial functions
void serial_init(void);
void serial_setup(int speed, int flags);
void serial_write(char c);
int serial_read(void);
bool serial_avail(void);

// time functions
int32_t time_getms(void);
void time_setms(int32_t time);


#endif /* HAL_H */