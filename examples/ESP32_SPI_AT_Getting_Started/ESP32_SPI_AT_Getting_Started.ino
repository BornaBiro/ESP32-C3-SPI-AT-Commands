// Include library for AT Commands over SPI.
#include "esp32SpiAt.h"

// Include Typedefs for the AT SPI messages.
#include "WiFiSPITypedef.h"

// Change WiFi SSID and password here.
#define WIFI_SSID   "Soldered-testingPurposes"
#define WIFI_PASS   "Testing443"

// Select one of the HTTP links.
char httpUrl[] = {"https://example.com"};
// char httpUrl[] = {"https://raw.githubusercontent.com/BornaBiro/ESP32-C3-SPI-AT-Commands/main/lorem_ipsum_long.txt"};
// char httpUrl[] = {"https://raw.githubusercontent.com/BornaBiro/ESP32-C3-SPI-AT-Commands/main/lorem_ipsum.txt"};

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

        // No point going on with the code if we can init the ESP32.
        while (1)
        {
            delay(100);
        }
    }
    Serial.println("ESP32 Initialization OK!");
    
    // If using static IP or different DNS, you can set it here. Parameters that you want to keep default, use INADDR_NONE as parameter.
    //WiFi.config(IPAddress(192, 168, 71, 3), IPAddress(192, 168, 71, 1), IPAddress(255, 255, 255, 0), IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));

    // Set to Access Point to change the MAC Address.
    if (!WiFi.setMode(INKPLATE_WIFI_MODE_AP))
    {
        Serial.println("AP mode failed!");

        // No point going on with the code it we can't set WiFi mode.
        while (1)
        {
            delay(100);
        }
    }

    // Print out ESP32 MAC address.
    Serial.print("ESP32 MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Change MAC address to something else. Only can be used with SoftAP!
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

    // Scan the WiFi networks.
    Serial.println("WiFi network scan:");

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
            Serial.print(WiFi.ssid(i));
            Serial.println(WiFi.auth(i)?'*':' ');
        }
    }
    else
    {
        Serial.println("No networks found.");
    }

    // Connect to the WiFi network.
    Serial.print("Connecting to the wifi...");
    WiFi.begin("Soldered", "dasduino");
    while (!WiFi.connected())
    {
        Serial.print('.');
        delay(500);
    }
    Serial.println("connected!");

    // Prtint out IP config data.
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

    Serial.println("Trying to open a text file from the internet...");

    // Create the WiFi client object for HTTP request.
    WiFiClient client;
    
    // Since the file will be received in chunks, keeps track of the byte received.
    uint32_t totalSize = 0;

    // Try to open a web page.
    if (client.connect(httpUrl))
    {
        Serial.print("Connected, file size: ");
        Serial.print(client.size(), DEC);
        Serial.println("bytes");

        // Use blocking method to get all chunks of the HTTP.
        while (client.available())
        {
            if (client.available() > 0)
            {
                char myBuffer[2000];
                int n = client.read(myBuffer, sizeof(myBuffer));
                totalSize += n;

                // Print out the chunk.
                for (int i = 0; i < n; i++)
                {
                    Serial.print(myBuffer[i]);
                }
            }
        }
    }
    else
    {
        Serial.println("Failed to get the file");
    }

    // End client HTTP request (MUST HAVE otherwise any other AT command to the modem will fail).
    if (!client.end())
    {
        Serial.println("Client end problem");
    }

    Serial.print("\n\n\n\nTotal received bytes: ");
    Serial.println(totalSize, DEC);

    Serial.println("Done!");
}

void loop()
{
    // Empty...
}

