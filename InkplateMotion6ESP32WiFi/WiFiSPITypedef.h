#ifndef __WIFISPITYPEDEF_H__
#define __WIFISPITYPEDEF_H__

// ESP32 Commands for SPI protocol.
#define INKPLATE_ESP32_SPI_CMD_REQ_TO_SEND_DATA     0x01
#define INKPLATE_ESP32_SPI_CMD_REQ_SLAVE_INFO       0x02
#define INKPLATE_ESP32_SPI_CMD_MASTER_SEND          0x03
#define INKPLATE_ESP32_SPI_CMD_MASTER_READ_DATA     0x04
#define INKPLATE_ESP32_SPI_CMD_MASTER_SEND_DONE     0x07
#define INKPLATE_ESP32_SPI_CMD_MASTER_READ_DONE     0x08

// ESP32 Slave Status report defines.
#define INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE    0x01
#define INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE   0x02

// ESP32 dependant stuff.
#define INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER           4092
#define INKPLATE_ESP32_SPI_DATA_INFO_MAGIC_NUM              0xFE
#define INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS             2000ULL
#define INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_SHORT_MS       200ULL
#define INKPLATE_ESP32_SPI_CHUNKED_READ_TIMEOUT_MS          20ULL

struct spiAtCommandTypedef
{
    uint8_t cmd;
    uint8_t addr;
    uint8_t dummy;
    uint8_t *data;
};

union spiAtCommandDataInfoTypedef
{
    struct dataInfoStruct
    {
        uint8_t magicNumber;
        uint8_t sequence;
        uint16_t length;
    }elements;
    uint8_t bytes[4];
};

union spiAtCommandsSlaveStatusTypedef
{
    struct slaveStatusStruct
    {
        uint8_t status;
        uint8_t sequence;
        uint16_t length;
    }elements;
    uint8_t bytes[4];
};

#endif
