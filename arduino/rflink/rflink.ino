#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "cmdproc.h"
#include "editline.h"
#include "hal.h"

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
static uint8_t pkt[64];
static uint8_t have_data = 0;

void setup()
{
    Serial.begin(57600);
    Serial.println("RFLINK");

    // read node id from eeprom
    node_id = id_read();
    Serial.print("node id = ");
    Serial.println(node_id, DEC);

    // SPI init
    spi_init(1000000L, 0);
    // TODO BSI move this to HAL
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV8);
    SPI.begin();
    pinMode(10, OUTPUT);

    radio_init(node_id);
    Serial.print("radio_init = ");
    if (radio_init(node_id)) {
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }
}

// forward declaration
static int do_help(int argc, char *argv[]);

static int do_id(int argc, char *argv[])
{
    uint8_t node_id = id_read();
    if (argc >= 2) {
        node_id = atoi(argv[1]);
        Serial.print("Setting id to ");
        Serial.print(node_id);
        id_write(node_id);
        radio_init(node_id);
   }
   Serial.print("id = ");
   Serial.println(node_id);
}

static int do_ping(int argc, char *argv[])
{
    uint8_t node = 0xFF;
    if (argc >= 2) {
        node = atoi(argv[1]);
    }
    Serial.print("Sending ping to ");
    Serial.println(node, HEX);

    int idx = 0;
    pkt[idx++] = node;
    pkt[idx++] = node_id;
    pkt[idx++] = 0x01;
    have_data = idx;
}

static const cmd_t commands[] = {
    {"help",    do_help,    "lists all commands"},
    {"id",      do_id,      "[id] gets/sets the node id"},
    {"ping",    do_ping,    "[node] sends a ping to node"},
    {"", NULL, ""}
};

typedef struct {
    uint32_t time;
    uint8_t frame;
    uint8_t slot_offs;
    uint8_t slot_size;
} beacon_t;

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

static beacon_t beacon;

void loop()
{
    static char textbuffer[16];
    static int prev_sec = 0;
    static int count = 0;
    static unsigned long int next_send;

    // command processing
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (line_edit(c, textbuffer, sizeof(textbuffer))) {
            int res = cmd_process(commands, textbuffer);
        }
    }

    // radio processing
    unsigned long int m = millis();

    // do beacon processing if we are master
    if (node_id == 0) {
        int sec = m / 100;
        if (sec != prev_sec) {
            prev_sec = sec;
            // update beacon
            beacon.time = m;
            beacon.frame++;
            beacon.slot_offs = 10;
            beacon.slot_size = 10;
            // create packet
            uint8_t buf[16];
            int idx = 0;
            buf[idx++] = 0xFF;      // to: broadcast
            buf[idx++] = node_id;   // from: us
            buf[idx++] = 0x00;      // command?
            memcpy(&buf[idx], &beacon, sizeof(beacon));
            idx += sizeof(beacon);
            // send it
            radio_send_packet(idx, buf);
            // configure our own send slot
            next_send = m + beacon.slot_offs + (beacon.slot_size * node_id);
        }
    }

    // our send slot arrived and we have something to send?
    if ((m >= next_send) && (m < (next_send + 10))) {
        if (have_data > 0) {
            radio_send_packet(have_data, pkt);
            have_data = 0;
        }
    }

    // handle received packets
    if (radio_packet_avail()) {
        uint8_t len;
        uint8_t buf[64];
        radio_recv_packet(&len, buf, sizeof(buf));
        if (buf[2] == 0x00) {
            // beacon
            memcpy(&beacon, &buf[3], sizeof(beacon));
//            Serial.print("beacon: time=");
//            Serial.print(beacon.time);
//            Serial.print(",frame=");
//            Serial.println(beacon.frame);
            // determine next send time
            next_send = m + beacon.slot_offs + (beacon.slot_size * node_id);
        } else if (buf[2] == 0x01) {
            // ping received
            uint8_t node = buf[1];
            Serial.print("Got ping from ");
            Serial.print(node);
            Serial.println(", sending pong...");
            // prepare pong
            int idx = 0;
            pkt[idx++] = node;
            pkt[idx++] = node_id;
            pkt[idx++] = 0x02;      // pong
            have_data = idx;
        } else {
            Serial.print("Got:");
            for (int i = 0; i < len; i++) {
                uint8_t b = buf[i];
                Serial.print((b >> 4) & 0xF);
                Serial.print((b >> 0) & 0xF);
            }
            Serial.println(".");
        }
    }

}
