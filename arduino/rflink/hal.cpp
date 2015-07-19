#include "Arduino.h"

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

// time functions
int32_t time_getms(void);
void time_setms(int32_t time);
