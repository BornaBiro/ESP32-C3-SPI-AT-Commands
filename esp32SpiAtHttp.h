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
    WiFiClient();
    bool connect(const char *_url);
    int available(bool _blocking = true);
    uint16_t read(char *_buffer, uint16_t _len);
    char read();
    bool end();
    int size();
    bool addHeader(char *_header);

  private:
    int cleanHttpGetResponse(char *_buffer, uint16_t *_len);
    int getFileSize(char *_url, uint32_t _timeout);

    uint16_t _bufferLen = 0;
    char *_currentPos = NULL;
    char *_dataBuffer = NULL;
    uint32_t _fileSize = 0;
};

#endif