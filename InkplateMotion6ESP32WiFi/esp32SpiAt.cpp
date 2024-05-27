// Include header file.
#include "esp32SpiAt.h"

// Flag for the handshake for the ESP32. Use SPI MODE0, MSBFIRST data transfet with approx. SPI clock rate of 20MHz.
static volatile bool _esp32HandshakePinFlag = false;

// SPI Settings for ESP32.
static SPISettings _esp32AtSpiSettings(20000000ULL, MSBFIRST, SPI_MODE0);

// ISR for the ESP32 handshake pin. This will be called automatically from the interrupt.
static void esp32HandshakeISR()
{
    _esp32HandshakePinFlag = true;
}

/**
 * @brief Construct a new Wi-Fi Class:: Wi Fi Class object
 *
 */
WiFiClass::WiFiClass()
{
    // Empty...for now.
}

/**
 * @brief   Initializes ESP32-C3 Module. It powers up the module, sets it to factory
 *          settings, initializes WiFi radio and disables storing settings in NVM.
 *
 * @return  bool
 *          True - Initialization ok, ESP32 is ready.
 *          False - Initialization failed.
 */
bool WiFiClass::init()
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

    // Try to power on the modem. Return false if failed.
    if (!power(true))
        return false;

    // If everything went ok, return true.
    return true;
}

/**
 * @brief   Power up or powers down the ESP32 module.
 *
 * @param   bool _en
 *          true - Enable the ESP32 module.
 *          false - Disables the ESP32 module.
 * @return  bool
 *          true - Modem is successfully powered up.
 *          false - Modem failed to power up.
 */
bool WiFiClass::power(bool _en)
{
    if (_en)
    {
        // Enable the power to the ESP32.
        digitalWrite(INKPLATE_ESP32_PWR_SWITCH_PIN, HIGH);

        // Wait a little bit for the ESP32 to boot up.
        delay(100);

        // Wait for the EPS32 to be ready. It will send a handshake to notify master
        // To read the data - "\r\nready\r\n" packet. Since the handshake pin pulled high
        // with the external resistor, we need to wait for the handshake pin to go low first,
        // then wait for the proper handshake event.
        if (!isModemReady())
            return false;
        // Serial.println("Modem ready");

        // Try to ping modem. Return fail if failed.
        if (!modemPing())
            return false;
        // Serial.println("Ping OK");

        // Set ESP32 to its factory settings.
        //if (!systemRestore())
        //    return false;
        // Serial.println("Settings restore OK");

        // Initialize WiFi radio.
        if (!wiFiModemInit(true))
            return false;
        // Serial.println("WiFi Init ready");

        // Disable stroing data in NVM. Return false if failed.
        if (!storeSettingsInNVM(false))
            return false;
        // Serial.println("Store in NVM disabled");

        // Disconnect from any previous WiFi network.
        disconnect();
        // Serial.println("WiFi Disconnect ready");
    }
    else
    {
        // Disable the power to the modem.
        // Disable the power to the ESP32.
        digitalWrite(INKPLATE_ESP32_PWR_SWITCH_PIN, HIGH);

        // Wait a little bit for the ESP32 to power down.
        delay(100);
    }

    // Everything went ok? Return true.
    return true;
}

/**
 * @brief   Methods sends AT command to the modem. It check if the modem is ready to accept the command or not.
 *
 * @param   char *_atCommand
 *          AT commnds that will be sent to the modem.
 * @return  bool
 *          true - AT Command is successfully sent.
 *          false - AT Command send failed (modem not ready to accept the command).
 * @note    AT Command needs to have CRLF at the and. method won't at it at the end of the command.
 */
bool WiFiClass::sendAtCommand(char *_atCommand)
{
    // Get the data size.
    uint16_t _dataLen = strlen(_atCommand);

    // First make a request for data send.
    dataSendRequest(_dataLen, 1);

    // Read the slave status.
    uint8_t _slaveStatus = 0;
    _slaveStatus = requestSlaveStatus();

    // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE.
    if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE)
        return false;

    // Send the data.
    dataSend(_atCommand, _dataLen);

    // Send data end.
    dataSendEnd();

    return true;
}

/**
 * @brief   Methods waits the response from the ESP32. It check if the modem is
 *          requesting the data read from slave. After it received request,
 *          timeout triggers if the new data is not available after timeout value.
 *          Timeout time is measured after the last received packet or char.
 *
 * @param   char *_response
 *          Buffer where to store response.
 * @param   uint32_t _bufferLen
 *          length of the buffer for the response (in bytes, counting the null-terminating char).
 * @param   unsigned long _timeout
 *          Timeout value from the last received char or packet in milliseconds.
 * @return  bool
 *          true - Response has been received (no error handle for now).
 */
bool WiFiClass::getAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout)
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
            // Update the timeout!
            _timeoutCounter = millis();

            // Read the slave status.
            uint16_t _responseLen = 0;
            uint8_t _slaveStatus = requestSlaveStatus(&_responseLen);

            // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE
            if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE)
                return false;

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

            // Clear the flag.
            _esp32HandshakePinFlag = false;
        }
    }

    // Add null-terminating char.
    _response[_resposeArrayOffset] = '\0';

    // Evertything went ok? Return true!
    return true;
}

/**
 * @brief   Wait for the reponse form the modem. Method check if the modem is requesting a read from the
 *          master device. It will wait timeout value until for the response.
 *
 * @param   char *_response
 *          Buffer where to store response.
 * @param   uint32_t _bufferLen
 *          length of the buffer for the response (in bytes, counting the null-terminating char).
 * @param   unsigned long _timeout
 *          Timeout value until the packets start arriving.
 * @param   uint16_t *_rxLen
 *          Pointer to the variable ehere length of the receiveds data will be stored.
 * @return  bool
 *          true - Response has been received (no error handle for now).
 */
bool WiFiClass::getSimpleAtResponse(char *_response, uint32_t _bufferLen, unsigned long _timeout, uint16_t *_rxLen)
{
    // Timeout variable.
    unsigned long _timeoutCounter = 0;

    // Variable for the response string length.
    uint16_t _responseLen = 0;

    // Capture the time!
    _timeoutCounter = millis();

    // Now loop until the timeout occurs
    while (((unsigned long)(millis() - _timeoutCounter) < _timeout) && (!_esp32HandshakePinFlag))
        ;

    // If the timeout occured, return false.
    if (!_esp32HandshakePinFlag)
        return false;

    // Otherwise read the data.
    // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE
    uint8_t _slaveStatus = requestSlaveStatus(&_responseLen);

    // Check the slave status, if must be INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE
    if (_slaveStatus != INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE)
        return false;

    // Check if the buffer is large enough for the data.
    // If not, drop everything.
    if (_responseLen < _bufferLen)
    {
        // Read the data.
        dataRead(_response, _responseLen);
    }

    // Clear handshake pin.
    _esp32HandshakePinFlag = false;

    // Send read done.
    dataReadEnd();

    // Add null-terminating char if needed.
    if (_rxLen == NULL)
    {
        _response[_responseLen] = '\0';
    }
    else
    {
        *_rxLen = _responseLen;
    }

    // Evertything went ok? Return true!
    return true;
}

/**
 * @brief   Method pings the modem (sends "AT" command and waits for AT OK).
 *
 * @return  bool
 *          true - Modem is available (received AT OK).
 *          false - Modem is not available.
 */
bool WiFiClass::modemPing()
{
    // Send "AT" AT Command for Modem Ping.
    sendAtCommand((char *)esp32AtPingCommand);

    // Wait for the response from the modem.
    if (!getAtResponse((char *)_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL))
        return false;

    // Check if AT\r\n\r\nOK\r\n\r\n is received.
    if (strcmp((char *)_dataBuffer, esp32AtPingResponse) != 0)
        return false;

    // If everything went ok, return true.
    return true;
}

/**
 * @brief   Methods sends AT command to set factory settings to the ESP32.
 *
 * @return  bool
 *          true - Modem restored settings to it's factory values.
 *          false - Modem failed to restore settings.
 */
bool WiFiClass::systemRestore()
{
    // Calling this command will set ESP32 to its factory settings.
    if (!sendAtCommand((char *)esp32AtCmdSystemRestore))
        return false;

    // Try to get the response (echo on CMD).
    if (!getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 20ULL))
        return false;

    // Now wait for the "OK".
    if (!getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 2000ULL))
        return false;

    // Parse the response. It should return OK.
    if (strstr(_dataBuffer, esp32AtCmdResponseOK) == NULL)
        return false;

    // Wait for the modem to be ready.
    getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 2000ULL);
    getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 2000ULL);

    // Everything went ok? Return true.
    return true;
}

/**
 * @brief   Enable or disable storing settings into ESP32 NVM. By default, while power up, it is
 *          disabled.
 *
 * @param   bool _store
 *          true - Store settings into ESP32 NVM.
 *          false - Do not store settings into NVM.
 * @return  bool
 *          true - Command executed successfully.
 *          false - Command failed.
 */
bool WiFiClass::storeSettingsInNVM(bool _store)
{
    // Disable or enable storing data in NVM. By default, storing is enabled, but at the ESP
    // start up is disabled.

    // Make a AT Command depending on the choice of storing settings in NVM.
    sprintf(_dataBuffer, "AT+SYSSTORE=%d\r\n", _store ? 1 : 0);

    // Send AT Command. Return false if failed.
    if (!sendAtCommand(_dataBuffer))
        return false;

    // Wait for the response. Return false if failed.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 10ULL))
        return false;

    // Check for the respose. Try to find "\r\nOK\r\n". Return false if find failed.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL)
        return false;

    // Otherwise return true.
    return true;
}

/**
 * @brief   Get the pointer (address) of the buffer for the SPI data.
 *
 * @return  char*
 *          Pointer of the SPI buffer for ESP32 communication and commands.
 */
char *WiFiClass::getDataBuffer()
{
    // Return the main RX/TX buffer fopr SPI data.
    return _dataBuffer;
}

/**
 * @brief   Methods sets WiFi mode (null, station, SoftAP or station and SoftAP).
 *
 * @param   uint8_t _wifiMode
 *          Use preddifend macros (can be found in WiFiSPITypedef.h)
 *          INKPLATE_WIFI_MODE_NULL - Null mode (modem disabled)
 *          INKPLATE_WIFI_MODE_STA - Station mode
 *          INKPLATE_WIFI_MODE_AP - Soft Access Point (not implemeted yet!)
 *          INKPLATE_WIFI_MODE_STA_AP - Both station and Soft Access Point (AP mode still not implemeted!).
 * @return  bool
 *          true - Mode set successfully on ESP32
 *          false - Mode set failed.
 */
bool WiFiClass::setMode(uint8_t _wifiMode)
{
    // Check for the proper mode.
    if ((_wifiMode > INKPLATE_WIFI_MODE_STA_AP))
        return false;

    // Create AT Command string depending on the mode.
    sprintf(_dataBuffer, "AT+CWMODE=%d\r\n", _wifiMode);

    // Issue a AT Command for WiFi Mode.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return false;

    // Check for the result.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL)
        return false;

    // Now disconnect from the network.
    disconnect();

    // Otherwise, return ok.
    return true;
}

/**
 * @brief   Connect to the access point.
 *
 * @param   char *_ssid
 *          Char array/pointer to the AP name name.
 * @param   char *_pass
 *          Char array/pointer to the AP password.
 * @return  bool
 *          true - Command execution was successfull.
 *          false -  Command execution failed.
 * @note    Max characters for password is limited to 63 chars and SSID is only limited to UTF-8 encoding.
 *          Try to avoid usage following chars: {"}, {,}, {\\}. If used, escape char must be added.
 */
bool WiFiClass::begin(char *_ssid, char *_pass)
{
    // Check for user mistake (null-pointer!).
    if ((_ssid == NULL) || (_pass == NULL))
        return false;

    // Create string for AT comamnd.
    sprintf(_dataBuffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", _ssid, _pass);

    // Issue an AT Command to the modem.
    if (!sendAtCommand(_dataBuffer))
        false;

    // Do not wait for response eventhough modem will send reponse as soon as the WiFi connection is established.
    return true;
}

/**
 * @brief   Methods returns the status of the ESP32 WiFi connection the AP.
 *
 * @return  bool
 *          true - ESP32 is connected to the AP.
 *          false - ESP32 is not connected to the AP.
 */
bool WiFiClass::connected()
{
    // Flush AT Read Request.
    requestSlaveStatus();
    dataReadEnd();

    // Create AT Command string to check WiFi connection status.
    sprintf(_dataBuffer, "AT+CWSTATE?\r\n");

    // Issue a AT Command for WiFi Mode.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getSimpleAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return false;

    // Find start of the response. Return false if failed.
    char *_responseStart = strstr(_dataBuffer, "+CWSTATE:");
    if (_responseStart == NULL)
        return false;

    // Parse the data.
    int _result;
    if (sscanf(_responseStart, "+CWSTATE:%d", &_result) != 1)
        return false;

    if (_result == 2)
    {
        // Just classic questonable ESP32 things...must read WL CONNECTED, otherwise ESP32 hangs.
        getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 250ULL);

        // Return true fir connection success.
        return true;
    }

    return false;
}
/**
 * @brief   Method executes command to the ESP32 to disconnects from the AP.
 *
 * @return  bool
 *          true - Command is executed successfully.
 *          false - Command failed.
 */
bool WiFiClass::disconnect()
{
    // Sent AT command for WiFi Disconnect.
    sendAtCommand((char *)esp32AtWiFiDisconnectCommand);

    // Wait for the response from the modem.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return false;

    // Check if the proper response arrived from the modem.
    if (strcmp(_dataBuffer, esp32AtWiFiDisconnectresponse) != 0)
        return false;

    return true;
}

/**
 * @brief   Methods prompts ESP32 to run a WiFi network scan. Scan takes about 2 seconds.
 *
 * @return  int
 *          Number of available networks (including encrypted and hidden ones).
 */
int WiFiClass::scanNetworks()
{
    // Issue a WiFi Scan command.
    sendAtCommand((char *)esp32AtWiFiScan);

    // First the modem will echo back AT Command and do the disconnect.
    // If this does not happen, sometinhg is wrong, return error.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return 0;

    // Now wait for about 3 seconds for the WiFi scan to complete.
    // If failed for some reason, return error.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 2500UL))
        return 0;

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

/**
 * @brief   Method gets SSID of a scaned network.
 *
 * @param   int _ssidNumber
 *          Network number on the found network list.
 * @return  char*
 *          SSID of the network.
 */
char *WiFiClass::ssid(int _ssidNumber)
{
    // Check if the parsing is successfull. If not, return empty string.
    if (!parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData))
        return " ";

    // If parsing is successfull, return SSID name.
    return _lastUsedSsidData.ssidName;
}

/**
 * @brief   Method gets auth. status of the selected network after the scan.
 *
 * @param   int _ssidNumber
 *          Network number on the found network list.
 * @return  bool
 *          true - Network is encrypted.
 *          false - Network is open.
 */
bool WiFiClass::auth(int _ssidNumber)
{
    // Parse the found network data.
    parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData);

    // If parsing is successfull, return auth status (false = open network, true = password locked".
    return _lastUsedSsidData.authType ? true : false;
}

/**
 * @brief   Method gets the RRIS value of the selected scaned networtk.
 *
 * @param   int _ssidNumber
 *          Network number on the found network list.
 * @return  int
 *          RSSI value in dBm of the selected network.
 */
int WiFiClass::rssi(int _ssidNumber)
{
    // Parse the found network data.
    parseFoundNetworkData(_ssidNumber, &_lastUsedSsid, &_lastUsedSsidData);

    // If parsing is successfull, return RSSI.
    return _lastUsedSsidData.rssi;
}

/**
 * @brief   Obtain a local IP.
 *
 * @return  IPAddress
 *          Returns the local IP with Arduino IPAddress class.
 */
IPAddress WiFiClass::localIP()
{
    return ipAddressParse("ip");
}

/**
 * @brief   Get a gateway IP Address.
 *
 * @return  IPAddress
 *          Returns the Gateway IP with Arduino IPAddress class.
 */
IPAddress WiFiClass::gatewayIP()
{
    return ipAddressParse("gateway");
}

/**
 * @brief   Get a network subnet mask.
 *
 * @return  IPAddress
 *          Returns the network subnet mask with Arduino IPAddress class.
 */
IPAddress WiFiClass::subnetMask()
{
    return ipAddressParse("netmask");
}

/**
 * @brief   Get the DNS of the primary or secondary DNS.
 *
 * @param   uint8_t i
 *          0 - Get the DNS IP Address of the primary DNS.
 *          1 - Get the DNS IP Address of the secondary DNS.
 * @return  IPAddress
 *          Returns the selected DNS IP Address with Arduino IPAddress class.
 */
IPAddress WiFiClass::dns(uint8_t i)
{
    // Filter out the selected DNS. It can only be three DNS IP Addreses.
    if (i > 2)
        return INADDR_NONE;

    // DNS IP addresses array.
    int _dnsIpAddresses[3][4];

    // Clear the array.
    memset(_dnsIpAddresses[0], 0, sizeof(int) * 4);
    memset(_dnsIpAddresses[1], 0, sizeof(int) * 4);
    memset(_dnsIpAddresses[2], 0, sizeof(int) * 4);

    // Flag if the static or dynamic IP is used on ESP32.
    int _dhcpFlag = 0;

    // Issue a AT Command for DNS. Return invalid IP address if failed.
    if (!sendAtCommand((char *)esp32AtGetDns))
        return INADDR_NONE;

    // Wait for the response. If timed out, return invalid IP address.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return INADDR_NONE;

    // Try to parse the data.
    char *_responseStart = strstr(_dataBuffer, "+CIPDNS:");

    // If not found, return invalid IP Address.
    if (_responseStart == NULL)
        INADDR_NONE;

    // Parse it!
    int _res = sscanf(_responseStart, "+CIPDNS:%d,\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"", &_dhcpFlag,
                      &_dnsIpAddresses[0][0], &_dnsIpAddresses[0][1], &_dnsIpAddresses[0][2], &_dnsIpAddresses[0][3],
                      &_dnsIpAddresses[1][0], &_dnsIpAddresses[1][1], &_dnsIpAddresses[1][2], &_dnsIpAddresses[1][3],
                      &_dnsIpAddresses[2][0], &_dnsIpAddresses[2][1], &_dnsIpAddresses[2][2], &_dnsIpAddresses[2][3]);

    // Check if all is parsed correctly, it should find at least one DNS. If not, return invalid IP Address.
    if (_res < 4)
        return INADDR_NONE;

    // Return wanted DNS.
    return IPAddress(_dnsIpAddresses[i][0], _dnsIpAddresses[i][1], _dnsIpAddresses[i][2], _dnsIpAddresses[i][3]);
}

/**
 * @brief   Get the MAC address of the ESP32. It will be returned with char array
 *          ("aa:bb:cc:11:22:33").
 *
 * @return  char*
 *          MAC Address of the ESP32.
 * @note    Original MAC can be changed with WiFiClass::macAddress(char *_mac) method.
 */
char *WiFiClass::macAddress()
{
    // Send AT Command for getting AT Command from the module.
    if (!sendAtCommand((char *)esp32AtWiFiGetMac))
        return _invalidMac;

    // Wait for the response from the modem.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return _invalidMac;

    // Try to parse it.
    char *_responseStart = strstr(_dataBuffer, "+CIPAPMAC:");

    // If proper response is not found, return with invalid MAC address.
    if (_responseStart == NULL)
        return _invalidMac;

    // Get the MAC address from the response.
    int _res = sscanf(_responseStart, "+CIPAPMAC:%[^\r\n]", _esp32MacAddress);

    // If parsing failed, return invalid MAC address.
    if (!_res)
        return _invalidMac;

    // Return parsed MAC address.
    return _esp32MacAddress;
}

/**
 * @brief   Method set the MAC address of the ESP32.
 *
 * @param   char *_mac
 *          Pointer to the char array of the new MAC address. String with the
 *          new MAC address must have "aa:bb:cc:dd:ee:ff" format.
 * @return  bool
 *          true - New MAC address is set.
 *          false - New MAC address set failed.
 * @note    MAC address only can be set if the WiFI mode is set to SoftAP mode.
 */
bool WiFiClass::macAddress(char *_mac)
{
    // Create a string for the new MAC address.
    sprintf(_dataBuffer, "AT+CIPAPMAC=\"%s\"\r\n", _mac);

    // Send AT Command. Return false if failed.
    if (!sendAtCommand(_dataBuffer))
        return false;

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 40ULL))
        return false;

    // Get the response. Try to find \r\n\OK\r\n. Return false if failed.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL)
        return false;

    // Otherwise return true.
    return true;
}

/**
 * @brief   Methods enables complete WiFi config. Set LocalIP, GatewayIP, Subnet Mask and DNS with one call.
 *          To keep original value of the one IP address, use INADDR_NONE as parameter.
 *
 * @param   IPAddress _staticIP
 *          Set the local IP Address. To keep the default one, use INADDR_NONE.
 * @param   IPAddress _gateway
 *          Set the gateway IP Address. To keep the default one, use INADDR_NONE.
 * @param   IPAddress _subnet
 *          Set the sunbet mask. To keep the default one, use INADDR_NONE.
 * @param   IPAddress _dns1
 *          Set the Primary DNS IP Address. To keep the default one, use INADDR_NONE.
 * @param   IPAddress _dns2
 *          Set the Secondary DNS IP Address. To keep the default one, use INADDR_NONE.
 * @return  bool
 *          true - New IP config is set.
 *          false - New IP config set failed.
 */
bool WiFiClass::config(IPAddress _staticIP, IPAddress _gateway, IPAddress _subnet, IPAddress _dns1, IPAddress _dns2)
{
    // Return value variable.
    bool _retValue = true;

    // First get the current settings since not all of above must be included.
    if (_staticIP == INADDR_NONE)
    {
        _staticIP = ipAddressParse("ip");
    }

    if (_gateway == INADDR_NONE)
    {
        _gateway = ipAddressParse("gateway");
    }

    if (_subnet == INADDR_NONE)
    {
        _subnet = ipAddressParse("netmask");
    }

    if (_dns1 == INADDR_NONE)
    {
        _dns1 = dns(0);
    }

    if (_dns1 == INADDR_NONE)
    {
        _dns1 = dns(1);
    }

    // Now send modified data.
    // Check if anything with the IP config have been modified. If so, send new settings.
    if ((_staticIP != INADDR_NONE) || (_gateway != INADDR_NONE) || (_subnet != INADDR_NONE))
    {
        // Send the AT commands for the new IP config.
        sprintf(_dataBuffer, "AT+CIPSTA=\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"\r\n", _staticIP[0],
                _staticIP[1], _staticIP[2], _staticIP[3], _gateway[0], _gateway[1], _gateway[2], _gateway[3],
                _subnet[0], _subnet[1], _subnet[2], _subnet[3]);

        // Send AT command.
        sendAtCommand(_dataBuffer);

        // Wait for the response.
        getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 50ULL);

        // Check for the response. Set return value to false is setting IP has failed.
        if (strstr(_dataBuffer, esp32AtCmdResponseOK) == NULL)
            _retValue = false;
    }

    // Check the same thing, but for DNS.
    if ((_dns1 != INADDR_NONE) || (_dns2 != INADDR_NONE))
    {
        // Create AT command for the DNS settings.
        sprintf(_dataBuffer, "AT+CIPDNS=1,\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"\r\n", _dns1[0], _dns1[1], _dns1[2], _dns1[3],
                _dns2[0], _dns2[1], _dns2[2], _dns2[3]);

        // Send AT command.
        sendAtCommand(_dataBuffer);

        // Wait for the response.
        getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 50ULL);

        // Check for the response. Set return value to false is setting IP has failed.
        if (strstr(_dataBuffer, esp32AtCmdResponseOK) == NULL)
            _retValue = false;
    }

    // Return true if everything went ok or false if something failed (IP config or DNS).
    return _retValue;
}

/**
 * @brief   Wait for the ESP32 handshake pin to trigger master request using digitalRead()
 *          (polling). This funciton is not used anymore.
 *
 * @param   uint32_t _timeoutValue
 *          Timeout value until the request from the ESP32 happens in milliseconds.
 * @param   _validState
 *          Is the trigger state for the handshake pin HIGH or LOW.
 * @return  bool
 *          true - Handshake pin trigger detected.
 *          false - Timeout, no handshake trigger detected.
 */
bool WiFiClass::waitForHandshakePin(uint32_t _timeoutValue, bool _validState)
{
    // Variable for the timeout. Also capture the current state.
    unsigned long _timeout = millis();

    // Read the current state of the handshake pin.
    bool _handshakePinState = digitalRead(INKPLATE_ESP32_HANDSHAKE_PIN);

    // Check if the handshake pin is already set.
    if (_handshakePinState == _validState)
        return true;

    // If not, wait for the valid pin state.
    do
    {
        // Read the new state of the pin.
        _handshakePinState = digitalRead(INKPLATE_ESP32_HANDSHAKE_PIN);

        // Wait a little bit.
        delay(1);
    } while (((unsigned long)(millis() - _timeout) < _timeoutValue) && (_handshakePinState != _validState));

    // Check the state of the timeout. If timeout occured, return false.
    if ((millis() - _timeout) >= _timeoutValue)
        return false;

    // Otherwise return true.
    return true;
}

/**
 * @brief   Wait for the ESP32 handshake pin to trigger master request using interrutps.
 *
 * @param   uint32_t _timeoutValue
 *          Timeout value until the request from the ESP32 happens in milliseconds.
 * @return  bool
 *          true - Handshake pin trigger detected.
 *          false - Timeout, no handshake trigger detected.
 */
bool WiFiClass::waitForHandshakePinInt(uint32_t _timeoutValue)
{
    // First, clear the flag status.
    _esp32HandshakePinFlag = false;

    // Variable for the timeout. Also capture the current state.
    unsigned long _timeout = millis();

    // Wait for the rising edge in Handshake pin.
    while (((unsigned long)(millis() - _timeout) < _timeoutValue) && (!_esp32HandshakePinFlag))
        ;

    // Clear the flag.
    _esp32HandshakePinFlag = false;

    // Check the state of the timeout. If timeout occured, return false.
    if ((millis() - _timeout) >= _timeoutValue)
        return false;

    // Otherwise return true.
    return true;
}

/**
 * @brief   Get ESP32 slave status. It is used right after the ESP32 sends handshake (ESP32 issues
 *          request from the master to read the data from the ESP32).
 *
 * @param   uint16_t _len
 *          Number of bytes requested waiting to be read by thje master from the ESP32.
 * @return  uint8_t
 *          Return slave status (INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE or
 *          INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE).
 */
uint8_t WiFiClass::requestSlaveStatus(uint16_t *_len)
{
    // Make a union/struct for data part of the SPI Command.
    union spiAtCommandsSlaveStatusTypedef _slaveStatus;
    _slaveStatus.elements.status = 0;
    _slaveStatus.elements.sequence = 0;
    _slaveStatus.elements.length = 0;

    // SPI Command Packet.
    struct spiAtCommandTypedef _spiCommand = {
        .cmd = INKPLATE_ESP32_SPI_CMD_REQ_SLAVE_INFO,
        .addr = 0x04,
        .dummy = 0x00,
        .data = (uint8_t *)&(_slaveStatus.bytes),
    };

    uint32_t _spiPacketLen = sizeof(_slaveStatus.bytes);

    // Transfer the packet!
    transferSpiPacket(&_spiCommand, _spiPacketLen);

    // Save the length if possible.
    if (_len != NULL)
        *_len = _slaveStatus.elements.length;

    // Return the slave status.
    return _slaveStatus.elements.status;
}

/**
 * @brief   Send data to the ESP32 and at the same time receive new data from ESP32.
 *
 * @param   spiAtCommandTypedef *_spiPacket
 *          Pointer to the spiAtCommandTypedef to describe data packet.
 * @param   uint16_t _spiDataLen
 *          length of the data part only, excluding spiAtCommandTypedef (in bytes).
 */
void WiFiClass::transferSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiDataLen)
{
    // Get the SPI STM32 HAL Typedef Handle.
    SPI_HandleTypeDef *_spiHandle = SPI.getHandle();

    // Activate ESP32 SPI lines by pulling CS pin to low.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET);

    // Send everything, but the data.
    SPI.beginTransaction(_esp32AtSpiSettings);
    SPI.transfer(_spiPacket->cmd);
    SPI.transfer(_spiPacket->addr);
    SPI.transfer(_spiPacket->dummy);

    // SPI.transfer(_spiPacket->data, _spiDataLen);
    HAL_SPI_TransmitReceive(_spiHandle, (uint8_t *)_spiPacket->data, (uint8_t *)_spiPacket->data, _spiDataLen,
                            HAL_MAX_DELAY);
    SPI.endTransaction();

    // Disable ESP32 SPI lines by pulling CS pin to high.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_SET);
}

/**
 * @brief   Method only sends SPI data to the ESP32.
 *
 * @param   spiAtCommandTypedef *_spiPacket
 *          Pointer to the spiAtCommandTypedef to describe data packet.
 * @param   uint16_t _spiDataLen
 *          length of the data part only, excluding spiAtCommandTypedef (in bytes).
 */
void WiFiClass::sendSpiPacket(spiAtCommandTypedef *_spiPacket, uint16_t _spiDataLen)
{
    // Get the SPI STM32 HAL Typedef Handle.
    SPI_HandleTypeDef *_spiHandle = SPI.getHandle();

    // Pack ESP32 SPI Packer Header data.
    uint8_t _esp32SpiHeader[] = {_spiPacket->cmd, _spiPacket->addr, _spiPacket->dummy};

    // Activate ESP32 SPI lines by pulling CS pin to low.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET);

    // Send everything, but the data.
    SPI.beginTransaction(_esp32AtSpiSettings);
    HAL_SPI_Transmit(_spiHandle, _esp32SpiHeader, sizeof(_esp32SpiHeader) / sizeof(uint8_t), HAL_MAX_DELAY);

    // Send data.
    HAL_SPI_Transmit(_spiHandle, _spiPacket->data, _spiDataLen, HAL_MAX_DELAY);
    SPI.endTransaction();

    // Disable ESP32 SPI lines by pulling CS pin to high.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_SET);
}

/**
 * @brief   Send data to the ESP32.
 *
 * @param   char *_dataBuffer
 *          Pointer to the data buffer.
 * @param   uint32_t _len
 *          length of the data (in bytes).
 * @return  bool
 *          true - Data sent successfully.
 */
bool WiFiClass::dataSend(char *_dataBuffer, uint32_t _len)
{
    // Before sending data:
    // 1. Make a request for data send
    // 2. Read and check slave status - It should return with INKPLATE_ESP32_SPI_SLAVE_STATUS_WRITEABLE.

    // Calculate the number of chunks to send, since the max is 4092 bytes.
    uint16_t _chunks = _len / INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER;
    uint16_t _lastChunk = _chunks ? _len % INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER : _len;

    // Address offset for the data packet.
    uint32_t _dataPacketAddrOffset = 0;

    // Create an data packet for data send.
    struct spiAtCommandTypedef _spiDataSend = {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_SEND, .addr = 0x00, .dummy = 0x00, .data = (uint8_t *)(_dataBuffer)};

    // Go trough the chunks.
    while (_chunks--)
    {
        // Calculate the chunk size.
        uint16_t _chunkSize = _chunks != 0 ? INKPLATE_ESP32_SPI_MAX_MESAGE_DATA_BUFFER : _lastChunk;

        // Transfer the data!
        sendSpiPacket(&_spiDataSend, _chunkSize);

        // Update the address position.
        _dataPacketAddrOffset += _chunkSize;

        // Update the SPI ESP32 packer header.
        _spiDataSend.data = (uint8_t *)(_dataBuffer + _chunkSize);

        // Data end cmd? Don't know...Needs to be checked!
    }

    // Write the last one chunk (or the only one if the _len < 4092).
    sendSpiPacket(&_spiDataSend, _lastChunk);

    // Return true for success.
    return true;
}

/**
 * @brief   Send command to the ESP32 indicating data send has ended.
 *
 * @return  bool
 *          true - Command send successfully.
 */
bool WiFiClass::dataSendEnd()
{
    // Create the structure for the ESP32 SPI.
    // Data field is not used here.
    struct spiAtCommandTypedef _spiDataSend = {.cmd = INKPLATE_ESP32_SPI_CMD_MASTER_SEND_DONE, .addr = 0, .dummy = 0};

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, 0);

    // Return true for success.
    return true;
}

/**
 * @brief   Read the data requested by the ESP32 with handshake pin.
 *
 * @param   char *_dataBuffer
 *          Pointer to the data buffer (where to store received data).
 * @param   _len
 *          Number of bytes needed to be read from the ESP32 (get this data from the slave status).
 * @return  bool
 *          true - Data read ok.
 */
bool WiFiClass::dataRead(char *_dataBuffer, uint16_t _len)
{
    // Before reading the data:
    // 1. Make a request for data read.
    // 2. Read and check slave status - It should return with INKPLATE_ESP32_SPI_SLAVE_STATUS_READABLE.

    // Create an data packet for data send.
    struct spiAtCommandTypedef _spiDataSend = {
        .cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DATA, .addr = 0x00, .dummy = 0x00, .data = (uint8_t *)(_dataBuffer)};

    // Read the last one chunk (or the only one if the _len < 4092).
    transferSpiPacket(&_spiDataSend, _len);

    // Return true for success.
    return true;
}

/**
 * @brief   Send ESP32 that no more data will be read. Must be
 *          sent after data read.
 *
 * @return  bool
 *          true - Command sent successfully.
 */
bool WiFiClass::dataReadEnd()
{
    // Create the structure for the ESP32 SPI.
    // Data field is not used here.
    struct spiAtCommandTypedef _spiDataSend = {.cmd = INKPLATE_ESP32_SPI_CMD_MASTER_READ_DONE, .addr = 0, .dummy = 0};

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, 0);

    // Return true for success.
    return true;
}

/**
 * @brief   Make a request to send data to the ESP32.
 *
 * @param   int _len
 * @param   _seqNumber
 *          Message sequnece number - Guess this is used if the message is chunked.
 * @return  bool
 *          true - Request sent successfully.
 */
bool WiFiClass::dataSendRequest(uint16_t _len, uint8_t _seqNumber)
{
    // Create the structure for the ESP32 SPI.
    // Data field data info field now (spiAtCommandDataInfoTypedef union).

    // First fill the spiAtCommandDataInfoTypedef union (aka. data info field in the data field).
    spiAtCommandDataInfoTypedef _dataInfo;
    _dataInfo.elements.magicNumber = INKPLATE_ESP32_SPI_DATA_INFO_MAGIC_NUM;
    _dataInfo.elements.sequence = _seqNumber;
    _dataInfo.elements.length = _len;

    struct spiAtCommandTypedef _spiDataSend = {
        .cmd = INKPLATE_ESP32_SPI_CMD_REQ_TO_SEND_DATA,
        .addr = 0,
        .dummy = 0,
        .data = (uint8_t *)&(_dataInfo.bytes),
    };

    // Transfer the packet! The re is not data field this time, so it's size is zero.
    transferSpiPacket(&_spiDataSend, sizeof(_dataInfo.bytes));

    // Wait for the handshake!
    bool _ret = waitForHandshakePinInt(200ULL);

    // Return the success status. If timeout occured, data read req. has failed.
    return _ret;
}

/**
 * @brief   Method waits for the ESP32 module to be ready after power up.
 *
 * @return  bool
 *          true - ESP32 module/modem is ready.
 *          false - ESP32 did not respond with "ready" message.
 * @note    Timeout for the reponse is 5 seconds.
 */
bool WiFiClass::isModemReady()
{
    if (waitForHandshakePinInt(5000ULL))
    {
        // Check for the request, since the Handshake pin is high.
        // Also get the data length.
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
            if (strcmp("\r\nready\r\n", (char *)_dataBuffer) != 0)
            {
                // Serial.println("modem ready parse error!");
                return false;
            }
        }
        else
        {
            // Serial.println("Wrong slave request");
            return false;
        }
    }
    else
    {
        // Serial.println("Handshake not detected");
        return false;
    }

    // Modem ready? Return true for success!
    return true;
}

/**
 * @brief   Method initializes or de-initializes WiFi radio.
 *
 * @param   bool _status
 *          true - Init WiFi radio.
 *          false - Disable WiFi radio.
 * @return  bool
 *          true - Command executed successfully.
 *          false - Command failed.
 */
bool WiFiClass::wiFiModemInit(bool _status)
{
    // Create a AT Commands String depending on the WiFi Initialization status.
    sprintf(_dataBuffer, "AT+CWINIT=%d\r\n", _status);

    // Send AT command to the modem.
    sendAtCommand(_dataBuffer);

    // Wait for the response.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 250ULL))
        return false;

    // Parse it. Check for the OK string.
    if (strstr(_dataBuffer, esp32AtCmdResponse) == NULL)
        return false;

    // Otherwise return true.
    return true;
}

/**
 * @brief   Helper method for parsing scaned WiFi networks data.
 *
 * @param   int8_t _ssidNumber
 *          Requested scaned nwtwork ID number (internal ID number, not linked to the WiFi network in any way).
 * @param   int8_t *_lastUsedSsidNumber
 *          Pointer tothe variable that holds the last selected ID number.
 * @param   struct spiAtWiFiScanTypedef *_scanData
 *          Can be found in WiFiSPITypedef.h, holds SSID name, auth flag, RSSI value.
 * @return  bool
 *          true - Scaned WiFi network data parsed successfully.
 *          false - Parsing failed.
 */
bool WiFiClass::parseFoundNetworkData(int8_t _ssidNumber, int8_t *_lastUsedSsidNumber,
                                      struct spiAtWiFiScanTypedef *_scanData)
{
    // Check if the last used SSID number matches current one. If so, do not need to parse anything.
    if (*_lastUsedSsidNumber == _ssidNumber)
        return true;

    // If not, check for the SSID number.
    if (_ssidNumber > _foundWiFiAp)
        return false;

    // Try to parse it!
    int _result = sscanf(_dataBuffer + _startApindex[_ssidNumber], "+CWLAP:(%d,\"%[^\",]\",%d", &_scanData->authType,
                         _scanData->ssidName, &_scanData->rssi);

    // Check for parsing. If 3 parameters have been found, parsing is successfull.
    if (_result != 3)
        return false;

    // Otherwise set current one SSID number as last used SSID number.
    *_lastUsedSsidNumber = _ssidNumber;

    // Return true as success.
    return true;
}

/**
 * @brief   Helper method used for parsing IP Addreses )local, gateway or subnet from the ESP32 message.
 *
 * @param   char *_ipAddressType
 *          Select IP data that will be parsed ("ip", "gateway" or "netmask").
 * @return  IPAddress
 *          Selected IP Address with the Arduino IPAddress Class.
 */
IPAddress WiFiClass::ipAddressParse(char *_ipAddressType)
{
    // Array for IP Address. For some reason, STM32 can't parse %hhu so int and %d must be used.
    int _ipAddress[4];

    // String for filtering IP Adresses from the response.
    char _ipAddressTypeString[30];
    char _ipAddressTypeStringShort[20];
    sprintf(_ipAddressTypeString, "+CIPSTA:%s:\"%%d.%%d.%%d.%%d\"", _ipAddressType);
    sprintf(_ipAddressTypeStringShort, "+CIPSTA:%s:", _ipAddressType);

    // Send the AT Commands for the current IP address.
    sendAtCommand((char *)esp32AtWiFiGetIP);

    // Wait for the response from the modem. If there is no response, return invalid IP address.
    if (!getAtResponse(_dataBuffer, INKPLATE_ESP32_AT_CMD_BUFFER_SIZE, 50ULL))
        return INADDR_NONE;

    // Try to parse IP Address.
    char *_ipAddressStart = strstr(_dataBuffer, _ipAddressTypeStringShort);

    // If could not find the start, return invalid IP address.
    if (_ipAddressStart == NULL)
        return INADDR_NONE;

    // Get the IP Address from the response.
    int _res =
        sscanf(_ipAddressStart, _ipAddressTypeString, &_ipAddress[0], &_ipAddress[1], &_ipAddress[2], &_ipAddress[3]);

    // If can't find 4 bytes, return invalid IP Address.
    if (_res != 4)
        return INADDR_NONE;

    // Return the IP Address.
    return IPAddress(_ipAddress[0], _ipAddress[1], _ipAddress[2], _ipAddress[3]);
}

// Decalre WiFi class to be globally available and visable.
WiFiClass WiFi;