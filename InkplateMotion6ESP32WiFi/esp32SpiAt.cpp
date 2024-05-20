// Include header file.
#include "esp32SpiAt.h"

// Flag for the handshake for the ESP32.
static volatile bool _esp32HandshakePinFlag = false;

// ISR for the ESP32 handshake pin. This will be called automatically from the interrupt.
static void esp32HandshakeISR()
{
    _esp32HandshakePinFlag = true;
}

// Define a constructor.
AtSpi::AtSpi()
{
    // Empty...for now!
}

bool AtSpi::init()
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

    // Set interrupt on handshake pin.
    attachInterrupt(digitalPinToInterrupt(INKPLATE_ESP32_HANDSHAKE_PIN), esp32HandshakeISR, RISING);

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

    // Try to ping modem. Return fail if failed.
    if (!modemPing()) return false;

    // Initialize WiFi radio.
    if (!wiFiModemInit(true)) return false;

    // If everything went ok, return true.
    return true;
}

void AtSpi::power(bool _en)
{

}

bool AtSpi::sendAtCommand(char *_atCommand)
{
    // Get the data size.
    uint16_t _dataLen = strlen(_atCommand);

    // First make a request for data send.
    dataSendRequest(_dataLen, 1);

    // Read the slave status.
    uint8_t _slaveStatus = 0;
    _slaveStatus = requestSlaveStatus();

    // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE.
    if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE) return false;

    // Send the data.
    dataSend(_atCommand, _dataLen);

    // Send data end.
    dataSendEnd();

    return true;
}

bool AtSpi::getAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout)
{
    // Timeout variable.
    unsigned long _timeoutCounter = 0;

    // Variable for the response array index offset.
    uint32_t _resposeArrayOffset = 0;

    // Capture the time!
    _timeoutCounter = millis();

    // Now loop until the timeout occurs
    while ((unsigned long)(millis() - _timeoutCounter) < _timeout)
    {
        // Wait for the response by checking the handshake pin.
        if (_esp32HandshakePinFlag)
        {
            // Clear the flag.
            _esp32HandshakePinFlag = false;

            // Update the timeout!
            _timeoutCounter = millis();

            // Read the slave status.
            uint16_t _responseLen = 0;
            uint8_t _slaveStatus = requestSlaveStatus(&_responseLen);

            // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE
            if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE) return false;

            // Check if there is enough free memory in the buffer. If there is still free memory,
            // get the response. Otherwise, drop everything.
            if ((_responseLen + _resposeArrayOffset) < _bufferLen)
            {
                // Read the data.
                dataRead((_response + _resposeArrayOffset), _responseLen);

                // Move the index in response array.
                _resposeArrayOffset += _responseLen;
            }

            // Send read done.
            dataReadEnd();
        }
    }

    // Add null-terminating char.
    _response[_resposeArrayOffset] = '\0';

    // Evertything went ok? Return true!
    return true;
}

bool AtSpi::modemPing()
{
    // Send "AT" AT Command for Modem Ping.
    sendAtCommand((char*)esp32AtPingCommand);

    // Wait for the response from the modem.
    if (!getAtResponse((char*)_dataBuffer, sizeof(_dataBuffer), 10ULL)) return false;

    // Check if AT\r\n\r\nOK\r\n\r\n is received.
    if (strcmp((char*)_dataBuffer, esp32AtPingResponse) != 0) return false;

    // If everything went ok, return true.
    return true;
}

bool AtSpi::setMode(uint8_t _wifiMode)
{
    // Check for the proper mode.
    if ((_wifiMode > INKPLATE_WIFI_MODE_STA_AP)) return false;

    // Create AT Command string depending on the mode.
    sprintf(_dataBuffer, "AT+CWMODE=%d\r\n", _wifiMode);

    // Issue a AT Command for WiFi Mode.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 200ULL)) return false;

    // Check for the result.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL) return false;

    // Otherwise, return ok.
    return true;
}

bool AtSpi::begin(char *_ssid, char* _pass)
{
    // Check for user mistake (null-pointer!).
    if ((_ssid == NULL) || (_pass == NULL)) return false;

    // Create string for AT comamnd.
    sprintf(_dataBuffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", _ssid, _pass);

    // Issue an AT Command to the modem.
    sendAtCommand(_dataBuffer);

    // Do not wait for response eventhough modem will send reponse as soon as the WiFi connection is established.
    return true;
}


bool AtSpi::connected()
{
    // Flush AT Read Request.
    requestSlaveStatus();
    dataReadEnd();

    // Create AT Command string to check WiFi connection status.
    sprintf(_dataBuffer, "AT+CWSTATE?\r\n");
    
    // Issue a AT Command for WiFi Mode.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 50ULL)) return false;

    // Find start of the response. Return false if failed.
    char *_responseStart = strstr(_dataBuffer, "+CWSTATE:");
    if (_responseStart == NULL) return false;

    // Parse the data.
    int _result;
    if (sscanf(_responseStart, "+CWSTATE:%d", &_result) != 1) return false;

    return (_result == 2)?true:false;
}

bool AtSpi::disconnect()
{
    // Sent AT command for WiFi Disconnect.
    sendAtCommand((char*)esp32AtWiFiDisconnectCommand);

    // Wait for the response from the modem.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 50ULL)) return false;

    // Check if the proper response arrived from the modem.
    if (strcmp(_dataBuffer, esp32AtWiFiDisconnectresponse) != 0) return false;

    return true;
}

int AtSpi::scanNetworks()
{
    // Issue a WiFi Scan command.
    sendAtCommand((char*)esp32AtWiFiScan);

    // First the modem will echo back AT Command and do the disconnect.
    // If this does not happen, sometinhg is wrong, return error.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 50UL)) return 0;

    // Now wait for about 3 seconds for the WiFi scan to complete.
    // If failed for some reason, return error.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 2500UL)) return 0;

    char *_wifiAPStart = strstr(_dataBuffer, "+CWLAP:");

    // Parse how many networks have been found.
    while (_wifiAPStart != NULL)
    {
        _startApindex[_foundWiFiAp] = _wifiAPStart - _dataBuffer;
        _foundWiFiAp++;
        _wifiAPStart = strstr(_wifiAPStart + 1, "+CWLAP:");
    }

    return _foundWiFiAp;
}

char* AtSpi::ssid(int _ssidNumber)
{
    // Check if the parsing is successfull. If not, return empty string.
    if (!parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData)) return " ";

    // If parsing is successfull, return SSID name.
    return _lastUsedSsidData.ssidName;
}

bool AtSpi::auth(int _ssidNumber)
{
    // Parse the found network data.
    parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData);

    // If parsing is successfull, return auth status (false = open network, true = password locked".
    return _lastUsedSsidData.authType?true:false;
}

int AtSpi::rssi(int _ssidNumber)
{
    // Parse the found network data.
    parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData);

    // If parsing is successfull, return RSSI.
    return _lastUsedSsidData.rssi;
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

bool AtSpi::waitForHandshakePinInt(uint32_t _timeoutValue)
{
    // First, clear the flag status.
    _esp32HandshakePinFlag = false;

    // Variable for the timeout. Also capture the current state.
    unsigned long _timeout = millis();

    // Wait for the rising edge in Handshake pin.
    while (((unsigned long)(millis() - _timeout) < _timeoutValue) && (!_esp32HandshakePinFlag));

    // Clear the flag.
    _esp32HandshakePinFlag = false;

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
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET);

    // Send everything, but the data.
    SPI.beginTransaction(SPISettings(20000000ULL, MSBFIRST, SPI_MODE0));
    SPI.transfer(_spiPacket->cmd);
    SPI.transfer(_spiPacket->addr);
    SPI.transfer(_spiPacket->dummy);
    
    SPI.transfer(_spiPacket->data, _spiDataLen);
    SPI.endTransaction();

    // Disable ESP32 SPI lines by pulling CS pin to high.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_SET);
}

bool AtSpi::dataSend(char *_dataBuffer, uint32_t _len)
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
        .data = (uint8_t*)(_dataBuffer)
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
        _spiDataSend.data = (uint8_t*)(_dataBuffer + _chunkSize);

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

bool AtSpi::dataRead(char *_dataBuffer, uint16_t _len)
{
    // Before reading the data:
    // 1. Make a request for data read.
    // 2. Read and check slave status - It should return with INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE.

    // Create an data packet for data send.
    struct spiAtCommandTypedef _spiDataSend = 
    {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DATA,
        .addr = 0x00,
        .dummy = 0x00,
        .data = (uint8_t*)(_dataBuffer)
    };

    // Read the last one chunk (or the only one if the _len < 4092).
    transferSpiPacket(&_spiDataSend, _len);

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

bool AtSpi::dataSendRequest(uint16_t _len, uint8_t _seqNumber)
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
        .cmd = INKPLATE_ESP32_SPI_CMD_REQ_TO_SEND_DATA,
        .addr = 0,
        .dummy = 0,
        .data = (uint8_t*)&(_dataInfo.bytes),
    };

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, sizeof(_dataInfo.bytes));

    // Wait for the handshake!
    bool _ret = waitForHandshakePinInt(INKPLATE_ESP32_SPI_HANDSHAKE_TIMEOUT_MS);

    // Return the success status. If timeout occured, data read req. has failed.
    return _ret;
}

bool AtSpi::isModemReady()
{
    if (waitForHandshakePinInt(5000ULL))
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

    // Modem ready? Return true for success!
    return true;
}

bool AtSpi::wiFiModemInit(bool _status)
{
    // Create a AT Commands String depending on the WiFi Initialization status.
    sprintf(_dataBuffer, "AT+CWINIT=%d\r\n", _status);

    // Send AT command to the modem.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, sizeof(_dataBuffer), 250ULL)) return false;

    // Parse it. Check for the OK string.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL) return false;

    // Otherwise return true.
    return true;
}

bool AtSpi::parseFoundNetworkData(int8_t _ssidNumber, int8_t *_lastUsedSsidNumber, struct spiAtWiFiScanTypedef *_scanData)
{
    // Check if the last used SSID number matches current one. If so, do not need to parse anything.
    if (*_lastUsedSsidNumber == _ssidNumber) return true;

    // If not, check for the SSID number.
    if (_ssidNumber > _foundWiFiAp) return false;

    // Try to parse it!
    int _result = sscanf(_dataBuffer + _startApindex[_ssidNumber], "+CWLAP:(%d,\"%[^\",]\",%d", &_scanData->authType, _scanData->ssidName, &_scanData->rssi);

    // Check for parsing. If 3 parameters have been found, parsing is successfull.
    if (_result != 3) return false;

    // Otherwise set current one SSID number as last used SSID number.
    *_lastUsedSsidNumber = _ssidNumber;

    // Return true as success.
    return true;
}