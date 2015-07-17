#include <arduino.h>
//#include <RH_RF69.h>

#include "SPI.h"
#include "radio.h"


// configuration for use of the band between 869.7 and 870.0 MHz

#define RADIO_FREQUENCY 869.85
#define RADIO_POWER     0

static uint8_t node_id;

bool radio_init(uint8_t address)
{
    node_id = address;

    // TODO BSI move this to HAL
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV8);
    SPI.begin();

    pinMode(10, OUTPUT);

    return true;
}

static void spi_select()
{
    digitalWrite(10, 0);
}


static void spi_deselect()
{
    digitalWrite(10, 1);
}


static void radio_write(uint8_t reg, uint8_t *data, int len)
{
    spi_select();

    SPI.transfer(reg);
    int i;
    for (i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }

    spi_deselect();
}

static void radio_read(uint8_t reg, uint8_t *data, int len)
{
    spi_select();
    SPI.transfer(reg);
    int i;
    for (i = 0; i < len; i++) {
        data[i] = SPI.transfer(0);
    }
    spi_deselect();
}

void radio_write_reg(uint8_t reg, uint8_t data)
{
    radio_write(reg | RFM69_WRITE_REG_MASK, &data, 1);
}

uint8_t radio_read_reg(uint8_t reg)
{
    uint8_t data;
    radio_read(reg, &data, 1);
    return data;
}

void radio_write_fifo(uint8_t *data, int len)
{
    radio_write(RFM69_FIFO | RFM69_WRITE_REG_MASK, data, len);
}

void radio_read_fifo(uint8_t *data, int len)
{
    radio_write(RFM69_FIFO, data, len);
}

// directly sends a packet
void radio_send_packet(uint8_t len, uint8_t *data)
{
    // set automode to start on fifo full
    radio_write_reg(RFM69_FIFO_THRESH, 0x02);

    // go to standby
    radio_write_reg(RFM69_OPMODE, RFM69_MODE_SEQUENCER_ON | RFM69_MODE_STANDBY);

    // perform automode
    radio_write_reg(RFM69_AUTO_MODES,
                    RFM69_AUTOMODE_ENTER_RISING_FIFOLEVEL |
                    RFM69_AUTOMODE_INTERMEDIATEMODE_TRANSMITTER |
                    RFM69_AUTOMODE_EXIT_RISING_PACKETSENT);

    radio_write_fifo(data, len);
}


bool radio_rf_init(void)
{
    // check version register
    uint8_t version = radio_read_reg(RFM69_VERSION);
    if (version != 0x24) {
        return false;
    }

    // packet mode, FSK, BT=0.5
    radio_write_reg(RFM69_DATA_MODUL, 0x02);

    //  bitrate
    radio_write_reg(RFM69_BITRATE_MSB, 0x01);
    radio_write_reg(RFM69_BITRATE_LSB, 0x00);

    // deviation
    radio_write_reg(RFM69_FDEV_MSB, 0x02);
    radio_write_reg(RFM69_FDEV_LSB, 0x00);

    // bandwidth
    radio_write_reg(RFM69_RX_BW, 0xE1);
    radio_write_reg(RFM69_AFC_BW, 0xE1);

    // fading margin improvement, see datasheet
    radio_write_reg(RFM69_TEST_DAGC, 0x30);

    // sync config
    radio_write_reg(RFM69_SYNC_CONFIG, (1 << 7) |   // sync on
                                       (0 << 6) |   // fifo fill condition
                                       (1 << 3) |   // sync size + 1
                                       (0 << 0));   // sync tolerance
    radio_write_reg(RFM69_SYNC_VALUE1, 0x2D);
    radio_write_reg(RFM69_SYNC_VALUE2, 0xD4);

    // preamble
    radio_write_reg(RFM69_PREAMBLE_MSB, 0);
    radio_write_reg(RFM69_PREAMBLE_LSB, 3);

    // power (use PA1, power = -18 + setting)
    radio_write_reg(RFM69_PA_LEVEL, (1 << 6) | 18);


    // packet config
    radio_write_reg(RFM69_PACKET_CONFIG1, (1 << 7) |    // packet format = variable
                                          (2 << 5) |    // whitening on
                                          (1 << 4) |    // CRC on
                                          (0 << 3) |    // CrcAutoClearOff
                                          (2 << 1));    // address filter = own or bcast
    radio_write_reg(RFM69_NODE_ADRESS, node_id);
    radio_write_reg(RFM69_BROADCAST_ADRESS, 0xFF);

    // LNA
    radio_write_reg(RFM69_LNA, (1 << 8));   // 200 ohm impedance, automatic gain

    // frequency = 869.85
    radio_write_reg(RFM69_FRF_MSB, 0xD9);
    radio_write_reg(RFM69_FRF_MID, 0x76);
    radio_write_reg(RFM69_FRF_LSB, 0x66);

    return true;
}

