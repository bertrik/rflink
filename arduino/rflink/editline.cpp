#include <stdbool.h>

#include <Arduino.h>

#define BELL    0x07
#define BS      0x08
#define LF      0x0A
#define CR      0x0D

/* Processes a character into an edit buffer, returns true 
 * @param c the character to process
 * @param buf the edit buffer
 * @param size the size of the buffer
 * @return true if a full line was entered, false otherwise
 */
bool line_edit(char c, char *buf, int size)
{
    static int index = 0;

    switch (c) {

    case CR:
    case LF:
        // finish
        buf[index] = 0;
        Serial.write(c);
        index = 0;
        return true;

    case BS:
    case 127:
        // backspace
        if (index > 0) {
            Serial.write(BS);
            Serial.write(' ');
            Serial.write(BS);
            index--;
        } else {
            Serial.write(BELL);
        }
        break;

    default:
        // try to add character to buffer
        if (index < (size - 1)) {
            buf[index++] = c;
            Serial.write(c);
        } else {
            Serial.write(BELL);
        }
        break;
    }
    return false;
}
