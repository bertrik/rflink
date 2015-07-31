#include "Arduino.h"
#include "SPI.h"
#include "EEPROM.h"

#include "hal.h"

// serial functions
void serial_init(uint32_t speed)
{
    Serial.begin(speed);
}

void serial_putc(char c)
{
    Serial.write(c);
}

int serial_getc(void)
{
    return Serial.read();
}

bool serial_avail(void)
{
    return (Serial.available() > 0);
}

// SPI functions
#define SPI_SELECT_PIN  10

void spi_init(uint32_t speed, int flags)
{
    (void)speed;    // ignored for now
    (void)flags;    // ignored for now

    pinMode(SPI_SELECT_PIN, OUTPUT);
    spi_select(false);

    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV8);
    SPI.begin();
}

void spi_select(bool enable)
{
    digitalWrite(SPI_SELECT_PIN, enable ? 0 : 1);
}

uint8_t spi_transfer(uint8_t in)
{
    return SPI.transfer(in);
}

// time functions
int32_t time_millis(void)
{
    return millis();
}

// non-volatile functions
uint8_t nv_read(int addr)
{
    return EEPROM.read(addr);
}

void nv_write(int addr, uint8_t data)
{
    EEPROM.write(addr, data);
}