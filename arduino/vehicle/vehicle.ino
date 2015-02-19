#include <SPI.h>
#include <RH_RF69.h>

#include "cmdproc.h"

// Singleton instance of the radio driver
static RH_RF69 rf69;

void setup()
{
    Serial.begin(9600);
    if (!rf69.init())
        Serial.println("init failed");
    else
        Serial.println("init success!");

    if (!rf69.setFrequency(869.850))
        Serial.println("setFrequency failed");

    const RH_RF69::ModemConfig config = {
        (RH_RF69_DATAMODUL_DATAMODE_PACKET |
         RH_RF69_DATAMODUL_MODULATIONTYPE_FSK |
         RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT0_5),
        0x01, 0x00,             // bit rate
        0x02, 0x00,             // freq dev
        0xe1, 0xe1,
        (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE |
         RH_RF69_PACKETCONFIG1_DCFREE_WHITENING |
         RH_RF69_PACKETCONFIG1_CRC_ON |
         RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NODE_BC)
    };
    rf69.setModemRegisters(&config);
    rf69.setTxPower(0);
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
