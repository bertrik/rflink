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
static int32_t time_offset = 0;

typedef struct {
    uint8_t len;
    uint8_t data[63];
} buffer_t;

static buffer_t buffers[9];

static void fill_buffer(uint8_t to, uint8_t flags, uint8_t len, uint8_t *data)
{
    // select our own node buffer as send buffer
    buffer_t *buf = &buffers[node_id];

    // fill it
    buf->len = 3 + len;
    buf->data[0] = to;
    buf->data[1] = node_id;
    buf->data[2] = flags;
    if (len > 0) {
        memcpy(&buf->data[3], data, len);
    }
}

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
        id_write(node_id);
        radio_init(node_id);
    }
    Serial.print("id 00 ");
    Serial.println(node_id);
    return 0;
}

static int do_ping(int argc, char *argv[])
{
    uint8_t node = 0xFF;
    if (argc >= 2) {
        node = atoi(argv[1]);
    }

    // prepare ping message
    fill_buffer(node, 0x01, 0, NULL);

    Serial.print(argv[0]);
    Serial.print(" 00 ");
    Serial.println(node, HEX);
    return 0;
}

static uint8_t hexdigit(char c)
{
    if (c >= 'a') {
        return c - 'a' + 10;
    } else if (c >= 'A') {
        return c - 'A' + 10;
    } else {
        return c - '0';
    }
}

static int decode_hex(char *s, uint8_t *buf, int size)
{
    int len = strlen(s);
    if ((len % 2) != 0) {
        return 0;
    }
    if ((len / 2) > size) {
        return 0;
    }
    for (int i = 0; i < len; i +=2) {
        uint8_t b = (hexdigit(s[i]) << 4) + hexdigit(s[i + 1]);
        *buf++ = b;
    }
    return len / 2;
}

static int do_send(int argc, char *argv[])
{
    if (argc != 4) {
        return -1;
    }
    uint8_t node = atoi(argv[1]);
    uint8_t type = atoi(argv[2]);
    uint8_t buf[64];
    int len = decode_hex(argv[3], buf, sizeof(buf));
    if (len < 0) {
        Serial.println("invalid hex");
        return -1;  // TODO BSI sensible error code
    }

    fill_buffer(node, type, len, buf);

    Serial.print(argv[0]);
    Serial.println(" 00");
    return 0;
}

static void printhex(uint8_t *rcv, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t b = rcv[i];
        Serial.print((b >> 4) & 0xF, HEX);
        Serial.print((b >> 0) & 0xF, HEX);
    }
}

static int do_recv(int argc, char *argv[])
{
    if (argc != 2) {
        return -1;
    }
    uint8_t node = atoi(argv[1]);
    buffer_t *buf = &buffers[node];
    if (buf->len == 0) {
        // nothing to read
        Serial.println("no data");
        return -1;
    }

    uint8_t dest = buf->data[0];
    uint8_t type = buf->data[2];

    Serial.print(argv[0]);
    Serial.print(" 00 ");
    Serial.print(dest, HEX);
    Serial.print(" ");
    Serial.print(type, HEX);
    Serial.print(" ");
    printhex(buf->data, buf->len);
    Serial.println("");

    return 0;
}

static int do_time(int argc, char *argv[])
{
    uint32_t time = millis() + time_offset;
    if (argc == 2) {
        time = atoi(argv[1]);
        // recalculate time offset
        time_offset = time - millis();
    }
    Serial.print(argv[0]);
    Serial.print(" 00 ");
    Serial.println(time);
}

static const cmd_t commands[] = {
    {"help",    do_help,    "lists all commands"},
    {"id",      do_id,      "[id] gets/sets the node id"},
    {"ping",    do_ping,    "[node] sends a ping to node"},
    {"s",       do_send,    "[node] [type] [data] sends data"},
    {"r",       do_recv,    "[node] returns data from buffer"},
    {"time",    do_time,    "[time] gets/set the time"},
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
            beacon.time = m + time_offset;
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
    if ((m >= next_send) && (m < (next_send + beacon.slot_size))) {
        buffer_t *buf = &buffers[node_id];
        if (buf->len > 0) {
            radio_send_packet(buf->len, buf->data);
            buf->len = 0;
        }
    }

    // handle received packets
    if (radio_packet_avail()) {
        uint8_t len;
        uint8_t rcv[64];
        radio_recv_packet(&len, rcv, sizeof(rcv));
        uint8_t node = rcv[1];
        uint8_t flags = rcv[2];
        switch (flags) {

        case 0x00:
            // decode beacon packet
            memcpy(&beacon, &rcv[3], sizeof(beacon));
            // determine next send time
            next_send = m + beacon.slot_offs + (beacon.slot_size * node_id);
            // recalculate time offset
            time_offset = beacon.time - m;
            break;

        case 0x01:
            // ping received
            Serial.print("!ping ");
            Serial.println(node, HEX);
            // prepare pong message
            fill_buffer(node, 0x02, 0, NULL);
            break;

        case 0x02:
            // pong received
            Serial.print("!pong ");
            Serial.println(node, HEX);
            break;

        default:
            // copy data into buffer and indicate reception
            buffer_t *buf = &buffers[node];  // TODO BSI check node in range
            memcpy(&buf->data, rcv, len);
            buf->len = len;

            Serial.print("!r ");
            Serial.print(node, HEX);
            Serial.print(" ");
            Serial.print(flags, HEX);
            Serial.print(" ");
            Serial.println(len);

            Serial.print("Got:");
            printhex(rcv, len);
            Serial.println(".");
            break;
        }
    }

}
