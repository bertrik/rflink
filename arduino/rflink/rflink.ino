#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "cmdproc.h"
#include "editline.h"

#include "SPI.h"
#include "EEPROM.h"

// EEPROM address of node id
#define EE_ADDR_ID  0

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


static uint8_t node_id;

void setup()
{
    Serial.begin(115200);
    Serial.println("RFLINK");

    node_id = id_read();
    Serial.print("node id = ");
    Serial.println(node_id, DEC);

    radio_init(node_id);
    Serial.print("init radio = ");
    if (radio_rf_init()) {
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }
}

// forward declaration
static int do_help(int argc, char *argv[]);

static const cmd_t commands[] = {
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


static char hello[] = "Hello Dear World, How Are You Today?";

void loop()
{
    static char textbuffer[16];

    long int t = millis();

    // command processing
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (line_edit(c, textbuffer, sizeof(textbuffer))) {
            int res = cmd_process(commands, textbuffer);
        }
    }

    // radio processing
    Serial.println("Sending packet ...");
    radio_send_packet(strlen(hello), (uint8_t *)hello);

    delay(10);
}
