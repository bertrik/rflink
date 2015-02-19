#include <SPI.h>
#include <RH_RF69.h>

// Singleton instance of the radio driver
static RH_RF69 rf69;

void setup()
{
    Serial.begin(9600);
    Serial.println("Receiver!");
    if (!rf69.init())
        Serial.println("init failed");
    else
        Serial.println("init success!");

    if (!rf69.setFrequency(869.850))
        Serial.println("setFrequency failed");

    const RH_RF69::ModemConfig config = {
        (RH_RF69_DATAMODUL_DATAMODE_PACKET |
         RH_RF69_DATAMODUL_MODULATIONTYPE_FSK |
         RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT0_5),
        0x01, 0x00, 
        0x02, 0x00, 
        0xe1, 0xe1,
        (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE |
         RH_RF69_PACKETCONFIG1_DCFREE_WHITENING |
         RH_RF69_PACKETCONFIG1_CRC_ON |
         RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)
    };
    rf69.setModemRegisters(&config);
    rf69.setTxPower(0);
    
    rf69.setModeRx();
}

static bool sendit(int len, const uint8_t *data, int retries)
{
    int i;
    for (i = 0; i < retries; i++) {
        int rssi = rf69.rssiRead();
        if (rssi < -80) {
            // channel clear, send it
            rf69.send(data, len);
            rf69.waitPacketSent();
            rf69.setModeRx();
            return true;
        }
        delay(2);
    }
    return false;
}


void loop()
{
    if (rf69.available()) {
        uint8_t buf[64];
        uint8_t len = sizeof(buf);
        rf69.recv(buf, &len);
        Serial.print(len, DEC);
        Serial.print(":");
        Serial.println((char *)buf);
    }
}
