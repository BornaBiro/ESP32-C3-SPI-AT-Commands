// Innclude main header file.
#include "esp32SpiAt.h"

// WiFiClient constructor - for HTTP.
WiFiClient::WiFiClient()
{
}

bool WiFiClient::connect(const char* _url)
{
    // Set data len to zero. And also file size.
    _bufferLen = 0;
    _fileSize = 0;

    // Get the RX Buffer Data Buffer pointer from the WiFi library.
    _dataBuffer = WiFi.getDataBuffer();

    // First set the message filter to set the modem in pass-trough mode.
    // Remove the header and "enter" at the end.
    if (!WiFi.sendAtCommand("AT+SYSMSGFILTERCFG=1,18,3\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;
    if (!WiFi.sendAtCommand("^+HTTPCGET:[0-9]*,\r\n$")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;
    if (!WiFi.sendAtCommand((char*)esp32AtCmdEscapeChar));
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Remove "OK" at the end.
    if (!WiFi.sendAtCommand("AT+SYSMSGFILTERCFG=1,0,7\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;
    if (!WiFi.sendAtCommand("\r\nOK\r\n$")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Enable the message filter.
    if (!WiFi.sendAtCommand("AT+SYSMSGFILTER=1\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Turn the Echo off.
    if (!WiFi.sendAtCommand("ATE0\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Try to get the file size. This also serves as connection to the client.
    _fileSize = getFileSize((char*)_url, 30000ULL);

    // Try to connect to the host. Return false if failed.
    sprintf(_dataBuffer, "AT+HTTPCGET=\"%s\",4096,4096,5000\r\n", _url);
    if (!WiFi.sendAtCommand(_dataBuffer)) return false;

    // // Wait for the response. Echo from sent command.
    // if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 10ULL)) return false;

    // Wait for the first data chunk. If timeout occured, return false.
    uint16_t _len = 0;
    if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 5000ULL, &_len)) return false;

    // Check for the "ERROR". If error is found, return false.
    //if (strstr(_dataBuffer, esp32AtCmdResponseError) != NULL) return false;

    //if (!cleanHttpGetResponse(_dataBuffer, &_bufferLen)) return false;
    _bufferLen += _len;
    _currentPos = _dataBuffer;

    return true;
}

int WiFiClient::available(bool _blocking)
{
    // Only get new data if the current buffer is empty.
    if (_bufferLen == 0)
    {
        // Calculate the timeout value for new data. If blocking method is enabled,
        // use longer timeout value. Otherwise, use shorter timeout value (but in this case user
        // must create some kind of mechanism to know when all data has been received).
        uint16_t _timeoutValue = _blocking?2500ULL:20UL;

        // Set variable for data chunk size to zero.
        uint16_t _len = 0;

        // Try to get new data. If new data is available, update the size and current pointer for the data.
        if (WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, _timeoutValue, &_len))
        {
            //if (strstr(_dataBuffer, esp32AtCmdResponseError) == NULL)
            //{
                //uint16_t _len = 0;
                //if (cleanHttpGetResponse(_dataBuffer, &_len))
                //{
                    _bufferLen += _len;
                    _currentPos = _dataBuffer;
                //}
            //}
        }
    }

    // Return the current buffer size.
    return _bufferLen;
}

uint16_t WiFiClient::read(char *_buffer, uint16_t _len)
{
    // If there is no new data, return 0.
    if (_currentPos == NULL) return 0;

    // Check if the buffer lenght is larger than received data.
    // If so, set the lenght to the received data lenght.
    if (_len > _bufferLen) _len = _bufferLen;

    // Copy data from internal buffer to the provided oone.
    memcpy(_buffer, _currentPos, _len);

    // Update the variables for offset and data lenght.
    _bufferLen -= _len;
    _currentPos += _len;

    // Return the actual lenght.
    return _len;
}

char WiFiClient::read()
{
    // Set the return variable default value.
    char _c = 0;

    // Check if there is any data left in the buffer.
    if (_bufferLen)
    {
        // read it and update the offset.
        _c = *(_currentPos++);

        // Update the buffer len (available data).
        _bufferLen--;
    }

    // Return the byte.
    return _c;
}

bool WiFiClient::end()
{
    // Clear all message filters used in http get.
    if (!WiFi.sendAtCommand("AT+SYSMSGFILTERCFG=0\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Disable the filter.
    if (!WiFi.sendAtCommand("AT+SYSMSGFILTER=0\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Turn on echo back.
    // Turn the Echo off.
    if (!WiFi.sendAtCommand("ATE1\r\n")) return false;
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL)) return false;

    // Everything went ok? Return true for success.
    return true;
}

int WiFiClient::size()
{
    // Return the file size.
    return _fileSize;
}

int WiFiClient::getFileSize(char *_url, uint32_t _timeout)
{
    int _size = 0;

    // Make a AT commands for the file size.
    sprintf(_dataBuffer, "AT+HTTPGETSIZE=\"%s\"\r\n", _url);

    // Send a AT commnds to the modem. Return 0 if failed.
    if (!WiFi.sendAtCommand(_dataBuffer)) return 0;

    // Try to get the response. Return 0 if failed.
    if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, _timeout)) return 0;

    // Wait a little bit. Otherwise modem fires "busy" message.
    delay(10);

    // Parse the reponse. Return 0 if something failed.
    if (strstr(_dataBuffer, "+HTTPGETSIZE:"))
    {
        Serial.println("Found them!");
        if (sscanf(_dataBuffer, "+HTTPGETSIZE:%d", &_size) != 1) return 0;
    }

    // Otherwise return file size.
    return _size;
}

int WiFiClient::cleanHttpGetResponse(char *_response, uint16_t *_cleanedSize)
{
    // Set the pointer for search.
    char *_startOfResponse = _response;
    
    // Set the variable for the complete length of the cleaned data.
    *_cleanedSize = 0;

    char *_writePointer = _response;
    
    while (_startOfResponse != NULL)
    {
        // Variable that holds the data part legnth of each chunk.
        int _dataChunkLen = 0;
        
        // Try to find the start of the HTTP response (+HTTPCGET:<len>,<data>CRLF).
        _startOfResponse = strstr(_startOfResponse, "+HTTPCGET:");

        // If nothing is found, stop the clean process.
        if (_startOfResponse == NULL) break;
        
        // Try to parse the length of the data part. If failed, move the pointer and keep looking.
        if (sscanf(_startOfResponse, "+HTTPCGET:%d,", &_dataChunkLen) == 1)
        {
            // Try to find the comma fter the lenght.
            char *_commaPos = strstr(_startOfResponse, ",");
        
            // If is found and it's 12 to 15 places after the start position it is valid.
            if ((_commaPos != NULL) && (_commaPos - _startOfResponse) <= 15)
            {
                // Check if is vaild. It must find \r\n at the end of the data.
                char _crChar = *(_commaPos + 1 + _dataChunkLen);
                char _lfChar = *(_commaPos + 1 + _dataChunkLen + 1);
                
                if ((_crChar == '\r') && (_lfChar == '\n')) 
                {
                    // Move all data at proper position.
                    memmove(_writePointer, _commaPos + 1, _dataChunkLen);
                    
                    // Update the variable for the cleaned data length.
                    (*_cleanedSize) += _dataChunkLen;
                    
                    // Move the write pointer.
                    _writePointer += _dataChunkLen;
                    
                }
            }
        }
        
        // move the pointer.
        _startOfResponse++;
    }

    if (*_cleanedSize == 0) return 0;

    return 1;
}