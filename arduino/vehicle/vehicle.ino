#include <stdint.h>

#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF69.h>

#include "cmdproc.h"
#include "editline.h"
#include "radio.h"

// EEPROM address of node id
#define EE_ADDR_ID  0

// Singleton instance of the radio driver
static RH_RF69 rf69;

static uint8_t id_read(void)
{
    return EEPROM.read(EE_ADDR_ID);
}

static void id_write(uint8_t id)
{
    uint8_t old = EEPROM.read(EE_ADDR_ID);
    if (id != old) {
        EEPROM.write(EE_ADDR_ID, id);
    }
}

void setup()
{
    Serial.begin(9600);
    Serial.println("Vehicle");

    uint8_t address = id_read();
    Serial.print("node id = ");
    Serial.println(address, DEC);
    
    Serial.print("init radio = ");
    if (radio_init(&rf69, address)) {
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }
    
    randomSeed(address);
}

// forward declaration
static int do_help(int argc, char *argv[]);

static int do_id(int argc, char *argv[])
{
    char id;
    if (argc > 1) {
        // set the id
        id = atoi(argv[1]);
        id_write(id);
    }
    // show the id
    id = id_read();
    Serial.print("id=");
    Serial.println(id, DEC);
}


static const cmd_t commands[] = {
    {"id",      do_id,      "[id] show or set the node id"},
    {"help",    do_help,    "lists all commands"},
    {"", NULL, ""}
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

static int fake_telemetry(long int t, uint8_t *buffer)
{
    int x = (int)(2000.0 * cos(t / 20000.0));
    int y = (int)(1000.0 * sin(t / 10000.0));

    buffer[0] = 0;
    buffer[1] = (x >> 8) & 0xFF;
    buffer[2] = (x >> 0) & 0xFF;
    buffer[3] = (y >> 8) & 0xFF;
    buffer[4] = (y >> 0) & 0xFF;
}


void loop()
{
    static long int last_sent = 0;
    uint8_t telemetry[5];

    long int m = millis();

    // command processing
    static char textbuffer[16];
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (line_edit(c, textbuffer, sizeof(textbuffer))) {
            int res = cmd_process(commands, textbuffer);
        }
    }
    
    // fake telemetry
    fake_telemetry(m, telemetry);
    
    // telemetry broadcast
    if ((m - last_sent) > 100) {
        // send it        
        if (!radio_broadcast(sizeof(telemetry), telemetry)) {
            // shift transmit schedule a bit
            last_sent += random(5);
        }
        last_sent += 100;
    }
}
