// Add headerguard do prevent multiple include.
#ifndef __ESP32_SPI_AT_HTTP_H__
#define __ESP32_SPI_AT_HTTP_H__

// Include main Arduino header file.
#include <Arduino.h>

// Include main ESP32-C3 AT SPI library.
#include "esp32SpiAt.h"

// Class for HTTP over SPI AT commands.
class WiFiClient
{
    public:
        // Constructor.
        WiFiClient();
        bool connect(const char* _url);
        int available();
        uint16_t read(char *_buffer, uint16_t _len);
        char read();
        bool end();

        private:
        int cleanHttpGetResponse(char *_buffer, uint16_t *_len);

        uint16_t _bufferLen = 0;
        char * _currentPos = NULL;
        char *_dataBuffer = NULL;
};

#endif