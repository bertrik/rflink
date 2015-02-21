#include <SPI.h>
#include <RH_RF69.h>

#include "radio.h"

// Singleton instance of the radio driver
static RH_RF69 rf69;

void setup()
{
    Serial.begin(9600);

    // init with special address 0 (roadside)
    radio_init(&rf69, 0);
}

static bool sendit(int len, const uint8_t *data, int retries)
{
    int i;
    for (i = 0; i < retries; i++) {
        int rssi = rf69.rssiRead();
        if (rssi < -80) {
            // channel clear, send it
            rf69.send(data, len);
            rf69.waitPacketSent();
            rf69.setModeRx();
            return true;
        }
        delay(2);
    }
    return false;
}


void loop()
{
    if (rf69.available()) {
        uint8_t buf[64];
        uint8_t len = sizeof(buf);
        rf69.recv(buf, &len);
        Serial.print(len, DEC);
        Serial.print(":");
        Serial.println((char *)buf);
    }
}
