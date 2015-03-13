#include <SPI.h>
#include <RH_RF69.h>

// configuration for use of the band between 869.7 and 870.0 MHz

#define RADIO_FREQUENCY 869.85
#define RADIO_POWER     0

static RH_RF69 *rf69;

static const RH_RF69::ModemConfig config = {
    (RH_RF69_DATAMODUL_DATAMODE_PACKET |
     RH_RF69_DATAMODUL_MODULATIONTYPE_FSK |
     RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT0_5),
    0x01, 0x00,             // bit rate
    0x02, 0x00,             // freq dev
    0xe1, 0xe1,
    (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE |
     RH_RF69_PACKETCONFIG1_DCFREE_WHITENING |
     RH_RF69_PACKETCONFIG1_CRC_ON |
     RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NODE_BC)
};

bool radio_init(RH_RF69 *rf, uint8_t address)
{
    rf69 = rf;

    if (!rf69->init()) {
        return false;
    }
    if (!rf69->setFrequency(RADIO_FREQUENCY)) {
        return false;
    }
    rf69->setModemRegisters(&config);
    rf69->setTxPower(RADIO_POWER);
    rf69->setThisAddress(address);
    rf69->setHeaderFrom(address);
    rf69->setModeRx();
    
    rf69->spiWrite(0x39, address);
    rf69->spiWrite(0x3A, 0xFF);
    
    return true;
}

// returns true if the radio channel is free
bool radio_clear(void)
{
    int rssi = rf69->rssiRead();
    return (rssi < -80);
}

// broadcasts a message if the channel is free
// returns true if the message was sent, false otherwise
bool radio_broadcast(int len, const uint8_t *data)
{
    rf69->setHeaderTo(0xFF);
    rf69->setHeaderId(0);
    
    rf69->send(data, len);
    rf69->waitPacketSent();
    
    // back to rx mode
    rf69->setModeRx();
    return true;
}



