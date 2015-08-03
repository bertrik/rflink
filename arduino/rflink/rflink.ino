#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rfm69.h"
#include "cmdproc.h"
#include "editline.h"
#include "hal.h"
#include "radio.h"

// Arduino needs these in the .ino file ...
#include "SPI.h"
#include "EEPROM.h"


// EEPROM address of node id
#define EE_ADDR_ID  0
// number of time division slots
#define NUM_SLOTS   9
// structure of a raw packet
#define PKT_OFFS_DST    0
#define PKT_OFFS_SRC    1
#define PKT_OFFS_TYPE   2
#define PKT_OFFS_DATA   3


// structure of a packet buffer
typedef struct {
    uint8_t len;
    uint8_t data[63];
} buffer_t;

// structure of a beacon packet
typedef struct {
    uint32_t time;
    uint8_t frame;
    uint8_t slot_offs;
    uint8_t slot_size;
} beacon_t;

// whether radio initialisation was successful
static boolean radio_ok = false;
// our node id
static uint8_t node_id;
// the current time offset between our clock and the master (milliseconds)
static int32_t time_offset = 0;
// latest received beacon packet
static beacon_t beacon;
// array of packet buffers, one for each node
static buffer_t buffers[NUM_SLOTS];


static void print(char *fmt, ...)
{
    // format it
    char buf[128];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, 128, fmt, args);
    va_end (args);

    // send it to serial
    char *p = buf;
    while (*p != 0) {
        serial_putc(*p++);
    }
}

static uint8_t id_read(void)
{
    return nv_read(EE_ADDR_ID);
}

static void id_write(uint8_t id)
{
    uint8_t old = nv_read(EE_ADDR_ID);
    if (id != old) {
        nv_write(EE_ADDR_ID, id);
    }
}

static bool node_valid(uint8_t id)
{
    return (id < NUM_SLOTS);
}

static void fill_buffer(uint8_t to, uint8_t flags, uint8_t len, uint8_t *data)
{
    // select our own node buffer as send buffer
    buffer_t *buf = &buffers[node_id];

    // fill it
    buf->len = 3 + len;
    buf->data[PKT_OFFS_DST] = to;
    buf->data[PKT_OFFS_SRC] = node_id;
    buf->data[PKT_OFFS_TYPE] = flags;
    if (len > 0) {
        memcpy(&buf->data[PKT_OFFS_DATA], data, len);
    }
}

static int do_id(int argc, char *argv[])
{
    uint8_t node = id_read();
    if (argc == 2) {
        node = atoi(argv[1]);
        if (!node_valid(node)) {
            return ERR_PARAM;
        }
        id_write(node);
        radio_init(node);
        node_id = node;
    }
    print("00 %02X\n", node_id);
    return 0;
}

static int do_ping(int argc, char *argv[])
{
    uint8_t node = ADDR_BROADCAST;
    if (argc > 1) {
        node = atoi(argv[1]);
    }

    // prepare ping message
    fill_buffer(node, PKT_TYPE_PING, 0, NULL);

    print("00 %02X\n", node);
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
        return ERR_PARAM;
    }
    uint8_t node = atoi(argv[1]);
    if (!node_valid(node)) {
        return ERR_PARAM;
    }
    uint8_t type = atoi(argv[2]);
    uint8_t buf[64];
    int len = decode_hex(argv[3], buf, sizeof(buf));
    if (len <= 0) {
        return ERR_PARAM;
    }

    fill_buffer(node, type, len, buf);

    print("00\n");
    return 0;
}

static void printhex(uint8_t *rcv, int len)
{
    for (int i = 0; i < len; i++) {
        print("%02X", rcv[i]);
    }
}

static int do_recv(int argc, char *argv[])
{
    if (argc != 2) {
        return ERR_PARAM;
    }
    uint8_t node = atoi(argv[1]);
    if (!node_valid(node)) {
        return ERR_PARAM;
    }
    buffer_t *buf = &buffers[node];
    if (buf->len == 0) {
        // nothing to read
        return ERR_NO_DATA;
    }

    uint8_t dest = buf->data[PKT_OFFS_DST];
    uint8_t type = buf->data[PKT_OFFS_TYPE];
    print("00 %02X %02X ", dest, type);
    printhex(&buf->data[PKT_OFFS_DATA], buf->len - PKT_OFFS_DATA);
    print("\n");

    // mark buffer as empty
    buf->len = 0;

    return 0;
}

static int do_time(int argc, char *argv[])
{
    uint32_t m = time_millis();
    uint32_t time = m + time_offset;
    if (argc == 2) {
        time = strtoul(argv[1], NULL, 0);
        // recalculate time offset
        time_offset = time - m;
    }
    print("00 %lu\n", time);
    return 0;
}

static int do_beacon(int argc, char *argv[])
{
    print("00 %lu %d %d %d\n", beacon.time, beacon.frame, beacon.slot_offs, beacon.slot_size);
    return 0;
}

static int do_status(int argc, char *argv[])
{
    print("00 ");
    for (int i = 0; i < NUM_SLOTS; i++) {
        print("%c", buffers[i].len > 0 ? '1' : '0');
    }
    print("\n");
    return 0;
}

static int do_power(int argc, char *argv[])
{
    if (argc != 2) {
        return ERR_PARAM;
    }
    int dbm = atoi(argv[1]);
    dbm = radio_set_power(dbm);
    // show actual power
    print("00 %d\n", dbm);
    return 0;
}

static int do_freq(int argc, char *argv[])
{
    if (argc != 2) {
        return ERR_PARAM;
    }
    uint32_t khz = strtoul(argv[1], NULL, 0);
    khz = radio_set_frequency(khz);
    print("00 %lu\n", khz);
    return 0;
}

// forward declaration of help function
static int do_help(int argc, char *argv[]);

static const cmd_t commands[] = {
    {"help",    do_help,    "lists all commands"},
    {"id",      do_id,      "[id] gets/sets the node id"},
    {"ping",    do_ping,    "[node] sends a ping to node"},
    {"s",       do_send,    "[node] [type] [data] sends data"},
    {"r",       do_recv,    "[node] returns data from buffer"},
    {"time",    do_time,    "[time] gets/set the time"},
    {"b",       do_beacon,  "shows current beacon info"},
    {"?",       do_status,  "shows current buffer status"},
    {"power",   do_power,   "<dbm> sets transmitter power"},
    {"freq",    do_freq,    "<khz> sets frequency"},
    {"", NULL, ""}
};

static int do_help(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    for (const cmd_t * cmd = commands; cmd->cmd != NULL; cmd++) {
        print("%s\t%s\n", cmd->name, cmd->help);
    }
}

void setup(void)
{
    serial_init(57600L);

    // read node id from eeprom
    node_id = id_read();

    // SPI init
    spi_init(1000000L, 0);

    radio_ok = radio_init(node_id);
    print("#RFLINK,id=%d,init=%s\n", node_id, radio_ok ? "OK" : "FAIL");
}

void loop(void)
{
    static char textbuffer[150];
    static int prev_sec = 0;
    static uint32_t next_send;

    // command processing
    if (serial_avail()) {
        char c = serial_getc();
        if (line_edit(c, textbuffer, sizeof(textbuffer))) {
            print("<");
            int res = cmd_process(commands, textbuffer);
            if (res < 0) {
                res = ERR_PARSE;;
            }
            if (res > 0) {
                print("%02X\n", res);
            }
        }
    }

    // verify conditions for doing any radio processing
    if (!radio_ok || !node_valid(node_id)) {
        return;
    }

    // radio processing
    uint32_t m = time_millis();

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
            buf[PKT_OFFS_DST] = ADDR_BROADCAST;
            buf[PKT_OFFS_SRC] = node_id;
            buf[PKT_OFFS_TYPE] = PKT_TYPE_BEACON;
            memcpy(&buf[PKT_OFFS_DATA], &beacon, sizeof(beacon));
            // send it
            radio_send_packet(PKT_OFFS_DATA + sizeof(beacon), buf);
            // configure our own send slot
            next_send = m + beacon.slot_offs + (beacon.slot_size * node_id);
        }
    }

    // our send slot arrived and we have something to send?
    if ((m >= next_send) && (m < (next_send + beacon.slot_size))) {
        buffer_t *buf = &buffers[node_id];
        if (buf->len > 0) {
            radio_send_packet(buf->len, buf->data);
            uint8_t node = buf->data[PKT_OFFS_DST];
            print("!s 00 %02X\n", node);
            buf->len = 0;
        }
    }

    // handle received packets
    if (radio_packet_avail()) {
        uint8_t len;
        uint8_t rcv[64];
        radio_recv_packet(&len, rcv, sizeof(rcv));
        uint8_t node = rcv[PKT_OFFS_SRC];
        uint8_t flags = rcv[PKT_OFFS_TYPE];
        switch (flags) {

        case PKT_TYPE_BEACON:
            // decode beacon packet
            memcpy(&beacon, &rcv[PKT_OFFS_DATA], sizeof(beacon));
            // determine next send time
            next_send = m + beacon.slot_offs + (beacon.slot_size * node_id);
            // recalculate time offset
            time_offset = beacon.time - m;
            break;

        case PKT_TYPE_PING:
            // ping received
            print("!ping %02X\n", node);
            // prepare pong message
            fill_buffer(node, PKT_TYPE_PONG, 0, NULL);
            break;

        case PKT_TYPE_PONG:
            // pong received
            print("!pong %02X\n", node);
            break;

        default:
            // copy data into buffer and indicate reception
            if (node_valid(node)) {
                buffer_t *buf = &buffers[node];
                if (buf->len == 0) {
                    // only indicate when buffer status changes (empty->full)
                    print("!r %02X\n", node);
                }
                memcpy(&buf->data, rcv, len);
                buf->len = len;
            }
            break;
        }
    }

}
