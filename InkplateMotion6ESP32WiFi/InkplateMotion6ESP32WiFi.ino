// Add an Inkplate Motion Libray to the Sketch.
#include <InkplateMotion.h>

// Include library for AT Commands over SPI.
#include "esp32SpiAt.h"

// Include Typedefs for the AT SPI messages.
#include "WiFiSPITypedef.h"

// Create an Inkplate Motion Object.
Inkplate inkplate;

void setup()
{
    // Setup a Serial communication for debug at 115200 bauds.
    Serial.begin(115200);

    // Print an welcome message (to know if the Inkplate board and STM32 are alive).
    Serial.println("Inkplate Motion Code Started!");

    // Initialize At Over SPI library.
    if (!WiFi.init())
    {
        Serial.println("ESP32-C3 initializaiton Failed! Code stopped.");

        while (1)
        {
            delay(100);
        }
    }
    Serial.println("ESP32 Initialization OK!");

    // Disconnect from all previous connections.
    WiFi.disconnect();

    // Set to Access Point to change the MAC Address.
    if (!WiFi.setMode(INKPLATE_WIFI_MODE_AP))
    {
        Serial.println("AP mode failed!");

        while (1)
        {
            delay(100);
        }
    }

    // Print out ESP32 MAC address.
    Serial.print("ESP32 MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Change MAC address to something else.
    if(!WiFi.macAddress("1a:bb:cc:01:23:45"))
    {
        Serial.println("MAC address Change failed!");
    }
    else
    {
        Serial.print("New MAC address: ");
        Serial.println(WiFi.macAddress());
    }

    // Set it back to the station mode.
    if (!WiFi.setMode(INKPLATE_WIFI_MODE_STA))
    {
        Serial.println("STA mode failed!");

        while (1)
        {
            delay(100);
        }
    }

    Serial.println("WiFi network scan:");
    // Scan the WiFi networks.
    int foundNetworks = WiFi.scanNetworks();
    if (foundNetworks != 0)
    {
        Serial.print("Found networks: ");
        Serial.println(foundNetworks, DEC);
        
        for (int i = 0; i < foundNetworks; i++)
        {
            Serial.print(i + 1, DEC);
            Serial.print(". RSSI: ");
            Serial.print(WiFi.rssi(i), DEC);
            Serial.print(' ');
            Serial.print(WiFi.auth(i)?'*':' ');
            Serial.println(WiFi.ssid(i));
        }
    }
    else
    {
        Serial.println("No networks found.");
    }

    Serial.print("Connecting to the wifi...");
    WiFi.begin("Soldered", "dasduino");
    while (!WiFi.connected())
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.println("connected!");

    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());

    for (int i = 0; i < 3; i++)
    {
        Serial.print("DNS ");
        Serial.print(i, DEC);
        Serial.print(": ");
        Serial.println(WiFi.dns(i));
    }

    WiFiClient myClient;
    //myClient.connect("http://example.com");
    //myClient.connect("http://meteo.hr");
    if (myClient.connect("https://raw.githubusercontent.com/BornaBiro/ESP32-C3-SPI-AT-Commands/main/lorem_ipsum.txt"))
    {
        Serial.println("Connected!");
        while (myClient.available())
        {
            if (myClient.available() > 0)
            {
                //char myBuffer[2000];
                //myClient.read(myBuffer, sizeof(myBuffer));
                //for (int i = 0; i < 64; i++)
                //{
                //    Serial.print(myBuffer);
                //}
               // Serial.flush();
               Serial.print(myClient.read());
            }
        }
    }

    Serial.println("\n\n\n\nDone!");

    //wifi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

    // if (!at.modemPing())
    // {
    //     Serial.println("Modem Ping Fail!");
    //     while (1);
    // }
    // else
    // {
    //     Serial.println("Modem Pink OK!");
    // }

    // if (!at.disconnect())
    // {
    //     Serial.println("WiFi disconnect Fail!");
    //     while (1);
    // }
    // else
    // {
    //     Serial.println("WiFi disconnected");
    // }

    // char rxBuffer[32768];
    // if (!at.sendAtCommand("AT\r\n", rxBuffer, 100ULL))
    // {
    //     Serial.println("AT Ping Fail!");
    // }
    // else
    // {
    //     Serial.println("AT Ping Ok, response:\r\n");
    //     Serial.println(rxBuffer);
    // }

    // if (!at.sendAtCommand("AT+CWINIT=1\r\n", rxBuffer, 100ULL))
    // {
    //     Serial.println("AT+CWINIT Fail!");
    // }
    // else
    // {
    //     Serial.println("AT+CWINIT, response:\r\n");
    //     Serial.println(rxBuffer);
    // }

    // if (!at.sendAtCommand("AT+CWMODE=1\r\n", rxBuffer, 100ULL))
    // {
    //     Serial.println("AT+CWMODE Fail!");
    // }
    // else
    // {
    //     Serial.println("AT+CWMODE, response:\r\n");
    //     Serial.println(rxBuffer);
    // }

    // if (!at.sendAtCommand("AT+CWLAP\r\n", rxBuffer, 10000ULL))
    // {
    //     Serial.println("AT+CWLAP Fail!");
    // }
    // else
    // {
    //     Serial.println("AT+CWLAP, response:\r\n");
    //     Serial.println(rxBuffer);
    // }
    // wifi.sendAtCommand("AT+CWJAP=\"Soldered\",\"dasduino\"\r\n");
    // if (!wifi.getAtResponse(rxBuffer, sizeof(rxBuffer), 2000ULL))
    // {
    //     Serial.println("AT+CWJAP Fail!");
    // }
    // else
    // {
    //     Serial.println("AT+CWJAP, response:\r\n");
    //     Serial.println(rxBuffer);
    // }
    // if (!at.sendAtCommand("AT+HTTPCGET=\"https://www.timeapi.io/api/TimeZone/AvailableTimeZones\"\r\n", rxBuffer, 10000ULL))
    // {
    //     Serial.println("AT+HTTPCGET Fail!");
    // }
    // else
    // {
    //     Serial.println("AT+HTTPCGET, response:\r\n");
    //     Serial.println(rxBuffer);
    // }


    // Initialize the Inkplate Motion Library.
    //inkplate.begin(INKPLATE_1BW);
}

void loop()
{

}

