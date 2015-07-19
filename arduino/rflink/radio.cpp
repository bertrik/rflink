#include "SPI.h"

#include "radio.h"

// configuration for use of the band between 869.7 and 870.0 MHz

#define RADIO_FREQUENCY 869.85
#define RADIO_POWER     0

static void spi_select()
{
    digitalWrite(10, 0);
}


static void spi_deselect()
{
    digitalWrite(10, 1);
}


static void radio_write(uint8_t reg, uint8_t * data, int len)
{
    spi_select();

    SPI.transfer(reg);
    int i;
    for (i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }

    spi_deselect();
}

static void radio_read(uint8_t reg, uint8_t * data, int len)
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

void radio_write_fifo(uint8_t * data, int len)
{
    radio_write(RFM69_FIFO | RFM69_WRITE_REG_MASK, data, len);
}

void radio_read_fifo(uint8_t * data, int len)
{
    radio_read(RFM69_FIFO, data, len);
}

void radio_mode_recv(void)
{
    // set mode to receive
    radio_write_reg(RFM69_OPMODE,
                    RFM69_MODE_SEQUENCER_ON | RFM69_MODE_RECEIVER);

#if 0
    // configure automode to go to standby on reception to read the packet
    radio_write_reg(RFM69_AUTO_MODES,
                    RFM69_AUTOMODE_ENTER_RISING_PAYLOADREADY |
                    RFM69_AUTOMODE_INTERMEDIATEMODE_STANDBY |
                    RFM69_AUTOMODE_EXIT_FALLING_FIFONOTEMPTY);
#else
    radio_write_reg(RFM69_AUTO_MODES, 0);
#endif
}

bool radio_packet_avail(void)
{
#if 1
    uint8_t irq2 = radio_read_reg(RFM69_IRQ_FLAGS2);
    // check PayloadReady
    return (irq2 & (1 << 2)) != 0;
#else
    // check AutoMode
    uint8_t irq1 = radio_read_reg(RFM69_IRQ_FLAGS1);
    return (irq1 & (1 << 1)) != 0;
#endif
}

bool radio_recv_packet(uint8_t * len_p, uint8_t * data, int size)
{
    // read length
    int len = radio_read_reg(RFM69_FIFO);
    if ((len == 0) || (len > size)) {
        return false;
    }
    // read data
    *len_p = len;
    for (int i = 0; i < len; i++) {
        data[i] = radio_read_reg(RFM69_FIFO);
    }

    return true;
}

// directly sends a packet
void radio_send_packet(uint8_t len, uint8_t * data)
{
    // set TxStartCondition to FifoNotEmpty
    radio_write_reg(RFM69_FIFO_THRESH, (1 << 7));

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
    radio_write_fifo(data, len);

    uint8_t irq1;
    // wait until automode exited
    do {
        irq1 = radio_read_reg(RFM69_IRQ_FLAGS1);
    } while ((irq1 & (1 << 1)) != 0);
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

    //  bitrate
    radio_write_reg(RFM69_BITRATE_MSB, 0x01);
    radio_write_reg(RFM69_BITRATE_LSB, 0x00);

    // deviation
    radio_write_reg(RFM69_FDEV_MSB, 0x02);
    radio_write_reg(RFM69_FDEV_LSB, 0x00);

    // bandwidth: setup for 250 kHz
    radio_write_reg(RFM69_RX_BW, (2 << 5) |     // DccFreq
                    (2 << 3) |  // 0:RxBwMant=16
                    (1 << 0));  // RxBwExp
    radio_write_reg(RFM69_AFC_BW, (2 << 5) |    // DccFreq
                    (2 << 3) |  // 0:RxBwMant=16
                    (1 << 0));  // RxBwExp

    // fading margin improvement, see datasheet
    radio_write_reg(RFM69_TEST_DAGC, 0x20);
    radio_write_reg(RFM69_AFC_CTRL, (1 << 5));  // AfcLowBetaOn

    // sync config
    radio_write_reg(RFM69_SYNC_CONFIG, (1 << 7) |       // sync on
                    (0 << 6) |  // fifo fill condition
                    (1 << 3) |  // sync size + 1
                    (0 << 0));  // sync tolerance
    radio_write_reg(RFM69_SYNC_VALUE1, 0x2D);
    radio_write_reg(RFM69_SYNC_VALUE2, 0xD4);

    // preamble (3 bytes)
    radio_write_reg(RFM69_PREAMBLE_MSB, 0);
    radio_write_reg(RFM69_PREAMBLE_LSB, 3);

    // power (use PA1, power = -18 + setting)
    radio_write_reg(RFM69_PA_LEVEL, (1 << 6) | 18);     // PA1ON
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

    // LNA
    radio_write_reg(RFM69_LNA, (1 << 8));       // 200 ohm impedance, automatic gain

    // frequency = 869850 kHz
    uint32_t f = 869850;
    uint32_t n = f * 2048 / 125;
    radio_write_reg(RFM69_FRF_MSB, (n >> 16) & 0xFF);
    radio_write_reg(RFM69_FRF_MID, (n >> 8) & 0xFF);
    radio_write_reg(RFM69_FRF_LSB, n & 0xFF);

    return true;
}
