// Add headerguard do prevent multiple include.
#ifndef __ESP32_SPI_AT_H__
#define __ESP32_SPI_AT_H__

// Add main Arduino header file.
#include <Arduino.h>

// include Arduino Library for the IP Adresses.
#include <IPAddress.h>

// Include Arduino SPI library.
#include <SPI.h>

// Include SPI AT Message typedefs.
#include "WiFiSPITypedef.h"

// Include file with all AT Commands.
#include "esp32SpiAtAllCommands.h"

// Include HTTP class for ESP32 AT Commands.
#include "esp32SpiAtHttp.h"

// Data buffer for AT Commands (in bytes).
#define INKPLATE_ESP32_AT_CMD_BUFFER_SIZE 8192ULL

// GPIO pin for the ESP32 Power Supply Switch.
#define INKPLATE_ESP32_PWR_SWITCH_PIN PG9

// GPIO Pins for the SPI communication with the ESP32.
#define INKPLATE_ESP32_MISO_PIN         PF8
#define INKPLATE_ESP32_MOSI_PIN         PF9
#define INKPLATE_ESP32_SCK_PIN          PF7
#define INKPLATE_ESP32_CS_PIN           PF6
#define INKPLATE_ESP32_HANDSHAKE_PIN    PA15

// Create class for the AT commands over SPI

class WiFiClass
{
    public:
        WiFiClass();

        // Public ESP32-C3 system functions.
        bool init();
        bool power(bool _en);
        bool sendAtCommand(char *_atCommand);
        bool getAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout);
        bool getSimpleAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout, uint16_t *_rxLen = NULL);
        bool modemPing();
        bool systemRestore();
        bool storeSettingsInNVM(bool _store);
        char* getDataBuffer();

        // Public ESP32 WiFi Functions.
        bool setMode(uint8_t _wifiMode);
        bool begin(char *_ssid, char* _pass);
        bool connected();
        bool disconnect();
        int scanNetworks();
        char* ssid(int _ssidNumber);
        bool auth(int _ssidNumber);
        int rssi(int _ssidNumber);
        IPAddress localIP();
        IPAddress gatewayIP();
        IPAddress subnetMask();
        IPAddress dns(uint8_t i);
        char* macAddress();
        bool macAddress(char *_mac);
        bool config(IPAddress _staticIP = INADDR_NONE, IPAddress _gateway = INADDR_NONE, IPAddress _subnet = INADDR_NONE, IPAddress _dns1 = INADDR_NONE, IPAddress _dns2 = INADDR_NONE);

    private:

        // ESP32 SPI Communication Protocol methods.
        bool waitForHandshakePin(uint32_t _timeoutValue, bool _validState = HIGH);
        bool waitForHandshakePinInt(uint32_t _timeoutValue);
        uint8_t requestSlaveStatus(uint16_t *_len = NULL);
        bool dataSend(char *_dataBuffer, uint32_t _len);
        bool dataSendEnd();
        bool dataRead(char *_dataBuffer, uint16_t _len);
        bool dataReadEnd();
        bool dataSendRequest(uint16_t _len, uint8_t _seqNumber);
        void transferSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiPacketLen);
        void sendSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiDataLen);
        // End of ESP32 SPI Communication Protocol methods.

        // Modem related methods.
        bool isModemReady();
        bool wiFiModemInit(bool _status);
        bool parseFoundNetworkData(int8_t _ssidNumber, int8_t *_lastUsedSsidNumber, struct spiAtWiFiScanTypedef *_scanData);
        IPAddress ipAddressParse(char *_ipAddressType);

        // Data buffer for the ESP32 SPI commands.
        char _dataBuffer[INKPLATE_ESP32_AT_CMD_BUFFER_SIZE];

        // Variables for WiFi Scan.
        int16_t _startApindex[40];
        uint8_t _foundWiFiAp = 0;
        int8_t _lastUsedSsid = -1;
        struct spiAtWiFiScanTypedef _lastUsedSsidData;
        char _invalidMac[18] = {"00:00:00:00:00:00"};
        // Array for storing parsed MAC address.
        char _esp32MacAddress[19];
};

// For easier user usage of the WiFi functionallity.
extern WiFiClass WiFi;

#endif