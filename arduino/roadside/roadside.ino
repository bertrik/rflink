#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <SPI.h>
#include <RH_RF69.h>

#include "radio.h"
#include "cmdproc.h"
#include "editline.h"

// Singleton instance of the radio driver
static RH_RF69 rf69;

typedef struct {
    boolean valid;
    uint8_t addr;
    long int time;
    int x;
    int y;
    int rssi;
} telem_t;

#define TELEM_SIZE  3
static telem_t telemetry[TELEM_SIZE];

void setup()
{
    Serial.begin(9600);
    Serial.println("Roadside");

    // init with special address 0 (roadside)
    Serial.print("init radio = ");
    if (radio_init(&rf69, 0)) {
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }    
    
    memset(&telemetry, 0, sizeof(telemetry));
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

static int telemetry_find_slot(long int t, uint8_t addr)
{
    // find by addr
    for (int i = 0; i < TELEM_SIZE; i++) {
        if (telemetry[i].valid && (telemetry[i].addr == addr)) {
            return i;
        }
    }
    // not found by addr, find unused slot
    for (int i = 0; i < TELEM_SIZE; i++) {
        if (!telemetry[i].valid) {
            return i;
        }
    }
    // no slot found
    return -1;
}

static void telemetry_expire(long int t)
{
    for (int i = 0; i < TELEM_SIZE; i++) {
        if (telemetry[i].valid && ((t - telemetry[i].time) > 3000)) {
            telemetry[i].valid = false;
        }
    }
}

static void telemetry_update(long int t, uint8_t addr, const uint8_t *buf, int rssi)
{
    int x = buf[1] * 256 + buf[2];
    int y = buf[3] * 256 + buf[4];

    // find an entry: match addr, otherwise re-use oldest
    int slot = telemetry_find_slot(t, addr);
    if (slot >= 0) {
        telemetry[slot].valid = true;
        telemetry[slot].addr = addr;
        telemetry[slot].time = t;
        telemetry[slot].x = x;
        telemetry[slot].y = y;
        telemetry[slot].rssi = rssi;
    }
}

static void telemetry_show(void)
{
    char buf[32];
    Serial.println("Id     X     Y RSSI time");
    for (int i = 0; i < TELEM_SIZE; i++) {
        telem_t *t = &telemetry[i];
        if (t->valid) {
            sprintf(buf, "%2d %5d %5d %4d %ld", t->addr, t->x, t->y, t->rssi, t->time);
            Serial.println(buf);
        }
    }
}

void loop()
{
    static char textbuffer[16];
    static long int last_sent = 0;
    static long int last_show = 0;
    
    long int t = millis();

    // command processing
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (line_edit(c, textbuffer, sizeof(textbuffer))) {
            int res = cmd_process(commands, textbuffer);
        }
    }
    
    // send beacon
    if (t > last_sent) {
        last_sent += 100;
        uint8_t buf[1];
        buf[0] = 0x01;
        radio_broadcast(sizeof(buf), buf);
Serial.print("#");
    }

    // telemetry reception
    if (rf69.available()) {
        uint8_t buf[64];
        uint8_t len = sizeof(buf);
        if (rf69.recv(buf, &len)) {
            if ((len > 0) && buf[0] == 0) {
                // got telemetry data
                uint8_t from = rf69.headerFrom();
                telemetry_update(t, from, buf, rf69.lastRssi());
            }
        }
Serial.print("!");
    }

    // telemetry display
    if ((t - last_show) > 1000) {
        Serial.println();
        telemetry_show();
        telemetry_expire(t);
        last_show = t;
    }
}
