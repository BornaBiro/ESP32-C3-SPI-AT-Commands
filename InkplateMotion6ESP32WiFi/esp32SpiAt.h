// Add headerguard do prevent multiple include.
#ifndef __ESP32_SPI_AT_H__
#define __ESP32_SPI_AT_H__

// Add main Arduino header file.
#include <Arduino.h>

// Include Arduino SPI library.
#include <SPI.h>

// Include SPI AT Message typedefs.
#include "WiFiSPITypedef.h"

// Include file with all AT Commands.
#include "esp32SpiAtAllCommands.h"

// GPIO pin for the ESP32 Power Supply Switch.
#define INKPLATE_ESP32_PWR_SWITCH_PIN PG9

// GPIO Pins for the SPI communication with the ESP32.
#define INKPLATE_ESP32_MISO_PIN         PF8
#define INKPLATE_ESP32_MOSI_PIN         PF9
#define INKPLATE_ESP32_SCK_PIN          PF7
#define INKPLATE_ESP32_CS_PIN           PF6
#define INKPLATE_ESP32_HANDSHAKE_PIN    PA15

// Create class for the AT commands over SPI

class AtSpi
{
    public:
        AtSpi();
        bool begin();
        void power(bool _en);
        bool sendAtCommand(char *_atCommand);
        bool getAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout);
        bool modemPing();
        bool wifiDisconnect();

    private:
        bool waitForHandshakePin(uint32_t _timeoutValue, bool _validState = HIGH);
        bool waitForHandshakePinInt(uint32_t _timeoutValue);
        uint8_t requestSlaveStatus(uint16_t *_len = NULL);
        bool dataSend(uint8_t *_dataBuffer, uint32_t _len);
        bool dataSendEnd();
        bool dataRead(uint8_t *_dataBuffer, uint16_t _len);
        bool dataReadEnd();
        bool dataSendRequest(uint16_t _len, uint8_t _seqNumber);

        bool readResponse(uint8_t *_buffer, uint32_t _bufferSize);

        void transferSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiPacketLen);
        bool isModemReady();

        // Data buffer for the ESP32 SPI commands.
        uint8_t _dataBuffer[4096];
};

#endif