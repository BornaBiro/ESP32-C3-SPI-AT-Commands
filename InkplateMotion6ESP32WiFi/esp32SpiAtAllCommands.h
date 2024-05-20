// Add a header guard.
#ifndef __ESP32_SPI_AT_ALL_COMMANDS_H__
#define __ESP32_SPI_AT_ALL_COMMANDS_H__

// static const char esp32AtSpiEmptyString[] = {" "};

// ESP32 System AT Commands
static const char esp32AtPingCommand[] = "AT\r\n";
static const char esp32AtPingResponse[] = "AT\r\n\r\nOK\r\n";
static const char esp32AtCmdResponse[] = "\r\n\r\nOK\r\n";

// ESP32 WiFi Commands
static const char esp32AtWiFiDisconnectCommand[] = "AT+CWQAP\r\n";
static const char esp32AtWiFiDisconnectresponse[] = "AT+CWQAP\r\n\r\nOK\r\n";
static const char esp32AtWiFiScan[] = "AT+CWLAP\r\n";
#endif