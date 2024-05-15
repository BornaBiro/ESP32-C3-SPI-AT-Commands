// Include header file.
#include "esp32SpiAt.h"

// // Flag for the handshake for the ESP32.
// static volatile bool _esp32HandshakePinFlag = false;

// // ISR for the ESP32 handshake pin. This will be called automatically from the interrupt.
// static void esp32HandshakeISR()
// {
//     _esp32HandshakePinFlag = true;
// }

// Define a constructor.
AtSpi::AtSpi()
{
    // Empty...for now!
}

bool AtSpi::begin()
{
    // Set the hardware level stuff first.
    
    // Set the SPI pins.
    SPI.setMISO(INKPLATE_ESP32_MISO_PIN);
    SPI.setMOSI(INKPLATE_ESP32_MOSI_PIN);
    SPI.setSCLK(INKPLATE_ESP32_SCK_PIN);

    // Initialize Arduino SPI Library.
    SPI.begin();

    // Set handshake pin.
    pinMode(INKPLATE_ESP32_HANDSHAKE_PIN, INPUT_PULLUP);

    // Set SPI CS Pin.
    pinMode(INKPLATE_ESP32_CS_PIN, OUTPUT);

    // Disable ESP32 SPI for now.
    digitalWrite(INKPLATE_ESP32_CS_PIN, HIGH);

    // Set ESP32 power switch pin.
    pinMode(INKPLATE_ESP32_PWR_SWITCH_PIN, OUTPUT);

    // Enable the power to the ESP32.
    digitalWrite(INKPLATE_ESP32_PWR_SWITCH_PIN, HIGH);

    // Wait a little bit for the ESP32 to boot up.
    delay(100);

    // Wait for the EPS32 to be ready. It will send a handshake to notify master
    // To read the data - "\r\nready\r\n" packet. Since the handshake pin pulled high
    // with the external resistor, we need to wait for the handshake pin to go low first,
    // then wait for the proper handshake event.
    if (!isModemReady()) return false;

    // If everything went ok, return true.
    return true;
}

void AtSpi::power(bool _en)
{

}

bool AtSpi::sendAtCommand(char *_atCommand, char *_response, unsigned long _timeout)
{
    // Timeout variable.
    unsigned long _timeoutCounter = 0;

    // Variable for the response array index offset.
    uint32_t _resposeArrayOffset = 0;

    // Get the data size.
    uint16_t _dataLen = strlen(_atCommand);

    // First make a request for data send.
    dataReadRequest(_dataLen, 1);

    // Wait for the handshake.
    if (!waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS)) return false;

    // Read the slave status.
    uint8_t _slaveStatus = 0;
    _slaveStatus = requestSlaveStatus();

    // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE.
    if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE) return false;

    // Send the data.
    dataSend((uint8_t*)_atCommand, _dataLen);

    // Send data end.
    dataSendEnd();

    // Wait until the handshake pin is set to low.
    waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS, LOW);

    // Capture the time!
    _timeoutCounter = millis();

    // Now loop until the timeout occurs
    while ((unsigned long)(millis() - _timeoutCounter) < _timeout)
    {
        // Wait for the response by reading the handshake pin.
        if (waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_SHORT_MS))
        {
            // Update the timeout!
            _timeoutCounter = millis();

            // Read the slave status.
            uint16_t _responseLen = 0;
            _slaveStatus = requestSlaveStatus(&_responseLen);

            // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE
            if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE) return false;

            // Read the data.
            dataRead((uint8_t*)(_response + _resposeArrayOffset), _responseLen);

            // Move the index in response array.
            _resposeArrayOffset += _responseLen;

            // Send read done.
            dataReadEnd();

            // Wait until the handshake pin is set to low.
            waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_SHORT_MS, LOW);
        }
    }

    // Add null-terminating char.
    _response[_resposeArrayOffset] = '\0';

    return true;
}

bool AtSpi::waitForHandshakePin(uint32_t _timeoutValue, bool _validState)
{
    // Variable for the timeout. Also capture the current state.
    unsigned long _timeout = millis();

    // Read the current state of the handshake pin.
    bool _handshakePinState = digitalRead(INKPLATE_ESP32_HANDSHAKE_PIN);

    // Check if the handshake pin is already set.
    if (_handshakePinState == _validState) return true;

    // If not, wait for the valid pin state.
    do
    {
        // Read the new state of the pin.
        _handshakePinState = digitalRead(INKPLATE_ESP32_HANDSHAKE_PIN);

        // Wait a little bit.
        delay(1);
    } while (((unsigned long)(millis() - _timeout) < _timeoutValue) && (_handshakePinState != _validState));

    // Check the state of the timeout. If timeout occured, return false.
    if ((millis() - _timeout) >= _timeoutValue) return false;

    // Otherwise return true.
    return true;
}

uint8_t AtSpi::requestSlaveStatus(uint16_t *_len)
{
    // Make a union/struct for data part of the SPI Command.
    union spiAtCommandsSlaveStatusTypedef _slaveStatus;
    _slaveStatus.elements.status = 0;
    _slaveStatus.elements.sequence = 0;
    _slaveStatus.elements.length = 0;

    // SPI Command Packet.
    struct spiAtCommandTypedef _spiCommand =
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_REQ_SLAVE_INFO,
        .addr = 0x04,
        .dummy = 0x00,
        .data = (uint8_t*)&(_slaveStatus.bytes),
    };

    uint32_t _spiPacketLen = sizeof(_slaveStatus.bytes);

    // Transfer the packet!
    transferSpiPacket(&_spiCommand, _spiPacketLen);

    // Save the length if possible.
    if (_len != NULL) *_len = _slaveStatus.elements.length;

    // Return the slave status.
    return _slaveStatus.elements.status;
}

void AtSpi::transferSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiDataLen)
{
    // Activate ESP32 SPI lines by pulling CS pin to low.
    digitalWrite(INKPLATE_ESP32_CS_PIN, LOW);

    // Send everything, but the data.
    SPI.beginTransaction(SPISettings(10000000ULL, MSBFIRST, SPI_MODE0));
    SPI.transfer(_spiPacket->cmd);
    SPI.transfer(_spiPacket->addr);
    SPI.transfer(_spiPacket->dummy);
    
    SPI.transfer(_spiPacket->data, _spiDataLen);
    SPI.endTransaction();

    // Disable ESP32 SPI lines by pulling CS pin to high.
    digitalWrite(INKPLATE_ESP32_CS_PIN, HIGH);
}

bool AtSpi::dataSend(uint8_t *_dataBuffer, uint32_t _len)
{
    // Before sending data:
    // 1. Make a request for data send
    // 2. Read and check slave status - It should return with INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE.

    // Calculate the number of chunks to send, since the max is 4092 bytes.
    uint16_t _chunks = _len / INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER;
    uint16_t _lastChunk = _chunks?_len % INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER:_len;

    // Address offset for the data packet.
    uint32_t _dataPacketAddrOffset = 0;

    // Create an data packet for data send.
    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_SEND,
        .addr = 0x00,
        .dummy = 0x00,
        .data = _dataBuffer
    };

    // Go trough the chunks.
    while (_chunks--)
    {
        // Calculate the chunk size.
        uint16_t _chunkSize = _chunks != 0?INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER:_lastChunk;

        // Transfer the data!
        transferSpiPacket(&_spiDataSend, _chunkSize);

        // Update the address position.
        _dataPacketAddrOffset+= _chunkSize;

        // Update the SPI ESP32 packer header.
        _spiDataSend.data = _dataBuffer + _chunkSize;

        // Data end cmd? Don't know...Needs to be checked!
    }

    // Write the last one chunk (or the only one if the _len < 4092).
    transferSpiPacket(&_spiDataSend, _lastChunk);

    // Return true for success.
    return true;
}

bool AtSpi::dataSendEnd()
{
    // Create the structure for the ESP32 SPI.
    // Data field is not used here.
    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_SEND_DONE,
        .addr = 0,
        .dummy = 0
    };

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, 0);

    // Return true for success.
    return true;
}

bool AtSpi::dataRead(uint8_t *_dataBuffer, uint32_t _len)
{
    // Before reading the data:
    // 1. Make a request for data read.
    // 2. Read and check slave status - It should return with INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE.

    // Calculate the number of chunks to send, since the max is 4092 bytes.
    uint16_t _chunks = _len / INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER;
    uint16_t _lastChunk = _chunks?_len % INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER:_len;

    // Address offset for the data packet.
    uint32_t _dataPacketAddrOffset = 0;

    // Create an data packet for data send.
    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DATA,
        .addr = 0x00,
        .dummy = 0x00,
        .data = _dataBuffer
    };

    // Go trough the chunks.
    while (_chunks--)
    {
        // Calculate the chunk size.
        uint16_t _chunkSize = _chunks != 0?INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER:_lastChunk;

        // Transfer the data!
        transferSpiPacket(&_spiDataSend, _chunkSize);

        // Update the address position and data.
        _dataPacketAddrOffset+= _chunkSize;

        // Update the SPI ESP32 packer header.
        _spiDataSend.data = _dataBuffer + _chunkSize;
        _spiDataSend.cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DATA;
        _spiDataSend.addr = 0x00;
        _spiDataSend.dummy = 0x00;

        // Data end cmd? Don't know...Needs to be checked!
    }

    // Read the last one chunk (or the only one if the _len < 4092).
    transferSpiPacket(&_spiDataSend, _lastChunk);

    // Return true for success.
    return true;
}

bool AtSpi::dataReadEnd()
{
    // Create the structure for the ESP32 SPI.
    // Data field is not used here.
    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DONE,
        .addr = 0,
        .dummy = 0
    };

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, 0);

    // Return true for success.
    return true;
}

bool AtSpi::dataReadRequest(uint16_t _len, uint8_t _seqNumber)
{
    // Create the structure for the ESP32 SPI.
    // Data field data info field now (spiAtCommandDataInfoTypedef union).

    // First fill the spiAtCommandDataInfoTypedef union (aka. data info field in the data field).
    spiAtCommandDataInfoTypedef _dataInfo;
    _dataInfo.elements.magicNumber = INKPLATE_ESP32_SPI_DATA_INFO_MAGIC_NUM;
    _dataInfo.elements.sequence = _seqNumber;
    _dataInfo.elements.length = _len;

    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_REQ_TO_READ_DATA,
        .addr = 0,
        .dummy = 0,
        .data = (uint8_t*)&(_dataInfo.bytes),
    };

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, sizeof(_dataInfo.bytes));

    // Wait for the handshake!
    bool _ret = waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS, true);

    // Return the success status. If timeout occured, data read req. has failed.
    return _ret;
}

bool AtSpi::isModemReady()
{
    if (waitForHandshakePin(5000ULL, LOW))
    {
        if (waitForHandshakePin(5000ULL, HIGH))
        {
            // Check for the request, since the Handshake pin is high.
            // Also get the data lenght.
            uint16_t _dataLen = 0;
            if (requestSlaveStatus(&_dataLen) == INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE)
            {
                // Ok, now try to read the data. First fill the ESP32 read packet
                dataRead(_dataBuffer, _dataLen);

                // Finish data read.
                dataReadEnd();

                // Parse the data!
                // Add null-terminating char at the end.
                _dataBuffer[_dataLen] = '\0';

                // Compare it. It should find "\r\nready\r\n".
                if (strcmp("\r\nready\r\n", (char*)_dataBuffer) != 0)
                {
                    Serial.println("modem ready parse error!");
                    return false;
                }
            }
            else
            {
                Serial.println("Wrong slave request");
                return false;
            }
        }
        else
        {
            Serial.println("Handshake not detected");
            return false;
        }
    }
    else
    {
        Serial.println("Handshake init fail.");
        return false;
    }

    // Wait until the handshake line is set to low.
    waitForHandshakePin(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS, LOW);

    // Modem ready? Return true for success!
    return true;
}