#include "hal.h"

#include "rfm69_const.h"
#include "rfm69.h"

// configuration for use of the band between 869.7 and 870.0 MHz

#define RADIO_FREQUENCY_KHZ 869850L
#define RADIO_POWER_DBM     0

// write data to a radio register
static void radio_write(uint8_t reg, const uint8_t * data, int len)
{
    spi_select(true);
    spi_transfer(reg);
    for (int i = 0; i < len; i++) {
        spi_transfer(data[i]);
    }
    spi_select(false);
}

// read data from a radio register
static void radio_read(uint8_t reg, uint8_t * data, int len)
{
    spi_select(true);
    spi_transfer(reg);
    for (int i = 0; i < len; i++) {
        data[i] = spi_transfer(0);
    }
    spi_select(false);
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

// go to receiver mode
static void radio_mode_recv(void)
{
    // set mode to receive
    radio_write_reg(RFM69_OPMODE,
                    RFM69_MODE_SEQUENCER_ON | RFM69_MODE_RECEIVER);

    radio_write_reg(RFM69_AUTO_MODES, 0);
}

bool radio_packet_avail(void)
{
    uint8_t irq2 = radio_read_reg(RFM69_IRQ_FLAGS2);
    // check PayloadReady
    return (irq2 & (1 << 2)) != 0;
}

bool radio_recv_packet(uint8_t * len_p, uint8_t * data, int size)
{
    // read length
    int len = radio_read_reg(RFM69_FIFO);
    if ((len == 0) || (len > size)) {
        return false;
    }
    *len_p = len;

    // read data
    radio_read(RFM69_FIFO, data, len);

    return true;
}

// directly sends a packet, blocks until packet sent
void radio_send_packet(uint8_t len, const uint8_t * data)
{
    // set mode to standby
    radio_write_reg(RFM69_OPMODE,
                    RFM69_MODE_SEQUENCER_ON | RFM69_MODE_STANDBY);

    // configure automode
    radio_write_reg(RFM69_AUTO_MODES,
                    RFM69_AUTOMODE_ENTER_RISING_FIFONOTEMPTY |
                    RFM69_AUTOMODE_INTERMEDIATEMODE_TRANSMITTER |
                    RFM69_AUTOMODE_EXIT_RISING_PACKETSENT);

    // start sending
    radio_write_reg(RFM69_FIFO, len);
    radio_write(RFM69_FIFO | RFM69_WRITE_REG_MASK, data, len);

    // wait until automode exited
    uint8_t irq1;
    do {
        irq1 = radio_read_reg(RFM69_IRQ_FLAGS1);
    } while ((irq1 & (1 << 1)) != 0);

    // back to receiver mode
    radio_mode_recv();
}

// set transmitter powers, returns actually configured power
int radio_set_power(int dbm)
{
    if (dbm < -2) {
        // 0.6 mW
        dbm = -2;
    }
    if (dbm > 13) {
        // 20 mW
        dbm = 13;
    }

    radio_write_reg(RFM69_PA_LEVEL, (1 << 6) | (dbm + 18));     // PA1ON
    return dbm;
}

// sets transmitter frequency, returns actually configured frequency
uint32_t radio_set_frequency(uint32_t khz)
{
    // restrict to SRD860 band (863-870 MHz)
    if (khz < 863000) {
        khz = 863000L;
    }
    if (khz > 870000) {
        khz = 870000L;
    }
    uint32_t n = ((khz * 2048) + 62) / 125;
    radio_write_reg(RFM69_FRF_MSB, (n >> 16) & 0xFF);
    radio_write_reg(RFM69_FRF_MID, (n >> 8) & 0xFF);
    radio_write_reg(RFM69_FRF_LSB, n & 0xFF);
    return khz;
}

bool radio_init(uint8_t node_id)
{
    // check version register
    uint8_t version = radio_read_reg(RFM69_VERSION);
    if (version != 0x24) {
        return false;
    }

    // packet mode, FSK, BT=0.5
    radio_write_reg(RFM69_DATA_MODUL, (0 << 5) |        // packet mode
                    (0 << 3) |  // FSK
                    (2 << 0));  // Gaussian filter, BT=0.5

    //  bitrate (bitrate = Fosc / value)
    radio_write_reg(RFM69_BITRATE_MSB, 0x01);
    radio_write_reg(RFM69_BITRATE_LSB, 0x00);

    // deviation (deviation = value * Fosc / 2^19)
    radio_write_reg(RFM69_FDEV_MSB, 0x02);
    radio_write_reg(RFM69_FDEV_LSB, 0x00);

    // bandwidth: setup for 250 kHz
    radio_write_reg(RFM69_RX_BW, (2 << 5) |     // DccFreq
                    (2 << 3) |  // 0:RxBwMant=16
                    (1 << 0));  // RxBwExp
    radio_write_reg(RFM69_AFC_BW, (2 << 5) |    // DccFreq
                    (2 << 3) |  // 0:RxBwMant=16
                    (1 << 0));  // RxBwExp

    // low beta settings
    radio_write_reg(RFM69_AFC_CTRL, (1 << 5));  // AfcLowBetaOn
    radio_write_reg(RFM69_TEST_DAGC, 0x20);     // fading margin improvement
    radio_write_reg(RFM69_TEST_AFC, 25);        // 0.04 * BW / 488 plus a bit

    // sync config
    radio_write_reg(RFM69_SYNC_CONFIG, (1 << 7) |       // sync on
                    (0 << 6) |  // fifo fill condition
                    (1 << 3) |  // sync size + 1
                    (0 << 0));  // sync tolerance
    radio_write_reg(RFM69_SYNC_VALUE1, 0x2D);
    radio_write_reg(RFM69_SYNC_VALUE2, 0xD4);

    // RSSI threshold (recommended value from datasheet)
    radio_write_reg(RFM69_RSSI_THRESH, 0xE4);

    // preamble
    radio_write_reg(RFM69_PREAMBLE_MSB, 0);
    radio_write_reg(RFM69_PREAMBLE_LSB, 4);

    // power amplifier
    radio_set_power(RADIO_POWER_DBM);
    radio_write_reg(RFM69_PA_RAMP, 9);  // 9 -> 40 us

    // packet config
    radio_write_reg(RFM69_PACKET_CONFIG1, (1 << 7) |    // packet format = variable
                    (2 << 5) |  // whitening on
                    (1 << 4) |  // CRC on
                    (0 << 3) |  // CrcAutoClearOff
                    (2 << 1));  // address filter: 2=own or bcast
    radio_write_reg(RFM69_PACKET_CONFIG2, (3 << 4) |    // interpacket rx delay, 2**X bits
                    (1 << 1));  // AutoRxRestartOn
    radio_write_reg(RFM69_PAYLOAD_LENGTH, 64);  // max payload length
    radio_write_reg(RFM69_NODE_ADRESS, node_id);
    radio_write_reg(RFM69_BROADCAST_ADRESS, 0xFF);

    // set TxStartCondition to FifoNotEmpty
    radio_write_reg(RFM69_FIFO_THRESH, (1 << 7));

    // LNA
    radio_write_reg(RFM69_LNA, (1 << 8));       // 200 ohm impedance, automatic gain

    // set frequency
    radio_set_frequency(RADIO_FREQUENCY_KHZ);

    // into receiver mode
    radio_mode_recv();

    return true;
}
