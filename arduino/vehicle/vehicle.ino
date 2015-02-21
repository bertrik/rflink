#include <stdint.h>

#include <SPI.h>
#include <RH_RF69.h>

#include "cmdproc.h"
#include "radio.h"

// Singleton instance of the radio driver
static RH_RF69 rf69;

void setup()
{
    // TODO get address from EEPROM
    uint8_t address = 1;
    radio_init(&rf69, address);
    
    Serial.begin(9600);
}

static bool sendit(int len, const uint8_t * data, int retries)
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

// forward declaration
static int do_help(int argc, char *argv[]);

static const cmd_t commands[] = {
    {"help", do_help, "lists all commands"}
};

static int do_help(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    for (const cmd_t * cmd = commands; cmd->cmd != NULL; cmd++) {
        Serial.print(cmd->name);
        Serial.print(" ");
        Serial.println(cmd->help);
    }
}


void loop()
{
    static long int last_sent = 0;

    long int m = millis();
    if ((m - last_sent) > 100) {
        last_sent += 100;

        // send with listen-before-talk
        char data[32];
        memset(data, 0, sizeof(data));
        strcpy(data, "THIS IS A BEACON");
        if (!sendit(sizeof(data), (uint8_t *) data, 3)) {
            last_sent += 3;
        }
    }
}
