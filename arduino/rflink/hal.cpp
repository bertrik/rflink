#include "Arduino.h"
#include "SPI.h"

#include "hal.h"

// serial functions
void serial_init(void)
{
    // nothing to do yet
}

void serial_setup(int speed, int flags)
{
    Serial.begin(speed);
}

void serial_write(char c)
{
    Serial.write(c);
}

int serial_read(void)
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
int32_t time_getms(void);
void time_setms(int32_t time);
