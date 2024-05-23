// Innclude main header file.
#include "esp32SpiAt.h"

// WiFiClient constructor - for HTTP.
WiFiClient::WiFiClient()
{
}

bool WiFiClient::connect(const char* _url)
{
    // Set data len to zero.
    _bufferLen = 0;

    // Get the RX Buffer Data Buffer pointer from the WiFi library.
    _dataBuffer = WiFi.getDataBuffer();

    // Try to connect to the host. Return false if failed.
    sprintf(_dataBuffer, "AT+HTTPCGET=\"%s\"\r\n", _url);
    if (!WiFi.sendAtCommand(_dataBuffer)) return false;

    // Wait for the response. Echo from sent command.
    if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 10ULL)) return false;

    // Wait for the first data chunk. If timeout occured, return false.
    if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 30000ULL)) return false;

    // Check for the "ERROR". If error is found, return false.
    if (strstr(_dataBuffer, esp32AtCmdResponseError) != NULL) return false;

    if (!cleanHttpGetResponse(_dataBuffer, &_bufferLen)) return false;

    _currentPos = _dataBuffer;

    return true;
}

int WiFiClient::available()
{
    if (_bufferLen == 0)
    {
        if (WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 5000ULL))
        {
            if (strstr(_dataBuffer, esp32AtCmdResponseError) == NULL)
            {
                uint16_t _len = 0;
                if (cleanHttpGetResponse(_dataBuffer, &_len))
                {
                    _bufferLen = _len;
                    _currentPos = _dataBuffer;
                }
            }
        }
    }

    return _bufferLen;
}

uint16_t WiFiClient::read(char *_buffer, uint16_t _len)
{
    if (_currentPos == NULL) return 0;

    if (_len > _bufferLen) _len = _bufferLen;
    memcpy(_buffer, _currentPos, _len);
    _bufferLen -= _len;
    _currentPos += _len;

    return _len;
}

char WiFiClient::read()
{
    char _c = 0;
    if (_bufferLen)
    {
        _c = *(_currentPos++);
        _bufferLen--;
    }

    return _c;
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