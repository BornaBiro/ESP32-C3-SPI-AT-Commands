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
    _offset = 0;

    // Get the RX Buffer Data Buffer pointer from the WiFi library.
    _dataBuffer = WiFi.getDataBuffer();

    // Try to connect to the host. Return false if failed.
    sprintf(_dataBuffer, "AT+HTTPCGET=\"%s\"\r\n", _url);
    if (!WiFi.sendAtCommand(_dataBuffer)) return false;

    // Wait for the response. Echo from sent command.
    if (!WiFi.getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 10ULL)) return false;

    Serial.println("[connect] echo ok");

    // Wait for the first data chunk. If timeout occured, return false.
    if (!WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 30000ULL)) return false;

    Serial.println("[connect] first chunk ok");


    // Check for the "ERROR". If error is found, return false.
    if (strstr(_dataBuffer, esp32AtCmdResponseError) != NULL) return false;

    Serial.println("[connect] no error respose found");
            //     for (int i = 0 ; i < 100; i++)
            // {
            //     Serial.print(_dataBuffer[i]);
            // }

    if (!cleanHttpGetResponse(_dataBuffer, &_bufferLen)) return false;

    Serial.print("[connect] http clean ok, data len: ");
    Serial.println(_bufferLen, DEC);

    return true;
}

int WiFiClient::available()
{
    if (_bufferLen == 0)
    {
        if (WiFi.getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 100ULL))
        {
            if (strstr(_dataBuffer, esp32AtCmdResponseError) == NULL)
            {
                uint16_t _len = 0;
                if (cleanHttpGetResponse(_dataBuffer, &_len))
                {
                    _bufferLen = _len;
                    _offset = 0;
                }
                else
                {
                    //Serial.println("Failed to clean http response");
                }
            }
            else
            {
                //Serial.println("Some how the response is error!?!?!?!?");
            }
        }
        else
        {
            //Serial.println("No new data?!?!?!");
        }
    }

    return _bufferLen;
}

void WiFiClient::read(char *_buffer, uint16_t _len)
{
    if (_len > _bufferLen) _len = _bufferLen;
    memcpy(_buffer, _dataBuffer + _offset, _len);
    _bufferLen -= _len;
    _offset+=_len;
}

char WiFiClient::read()
{
    char _c = 0;
    if (_bufferLen)
    {
        _c = *(_dataBuffer + _offset);
        _offset++;
        _bufferLen--;
    }

    return _c;
}

// bool WiFiClient::cleanHttpResponse(char *_buffer, uint16_t *_len)
// {
//     // Check for the pointers.
//     if ((_buffer == NULL) || (_len == NULL)) return false;

//     // Get the length of the response.
//     int _totalLen = strlen(_buffer);

//     // First test if this is really proper HTTPS response.
//     int _dataLen = 0;
//     char *_responseStart = strstr(_buffer, "+HTTPCGET:");
    
//     // Check if is not some data.
//     if (_responseStart != _buffer)
//     {
//         Serial.println("Can't find start");
//         return false;
//     }

//     // Try to parse the length of the data.
//     int _found = sscanf(_responseStart, "+HTTPCGET:%d,", &_dataLen);
//     if(_found != 1)
//     {
//         Serial.println("HTTP Len can't be found and parsed");
//         return false;
//     }

//     // Calculate the shift (to remove AT Command response and also CR LF at the end).
//     uint16_t _shift = _totalLen - _dataLen - 2;

//     // Check the shift length, it should be small. If the shift is large, must be something wrong.
//     if (_shift > 15)
//     {
//         Serial.println("\n\n\n\n");
//         Serial.print("Shift is too large! ");   
//         Serial.println(_shift, DEC);
//         Serial.print("_totalLen: ");   
//         Serial.println(_totalLen, DEC);
//         Serial.print("_dataLen: ");   
//         Serial.println(_dataLen, DEC);

//         Serial.println("\n\n----------------------");
//         Serial.println(_buffer);
//         Serial.println("\n\n----------------------");
//         return false;
//     }

//     // Move the response to the start.
//     memcpy(_buffer, _buffer + _shift, _dataLen);

//     // Add a terminating char at the end.
//     _buffer[_dataLen] = '\0';

//     Serial.print(_buffer);

//     // Copy the data length into the pointer.
//     *_len = _dataLen;

//     // Everything went ok? Return true for success.
//     return true;
// }

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
                // Try to find the CRLF.
                char *_crlfPos = strstr(_commaPos, "\r\n");
            
                // Check if is vaild. It must match the data length.
                if ((_crlfPos != NULL) && (_crlfPos - _commaPos - 1) == _dataChunkLen)
                {
                    //printf("Everything is ok! Start %.20s\r\n", _commaPos + 1);
                    
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