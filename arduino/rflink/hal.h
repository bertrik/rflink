/*
 * Simple abstraction layer above Arduino hardware
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>

// SPI functions
void spi_init(uint32_t speed, int flags);
void spi_select(bool enable);
uint8_t spi_transfer(uint8_t in);

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