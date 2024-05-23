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

    // Initialize the Inkplate Motion Library.
    inkplate.begin(INKPLATE_1BW);

    // Clear the screen.
    inkplate.display();

    // Set the text.
    inkplate.setCursor(0, 0);
    inkplate.setTextSize(1);
    inkplate.setTextColor(BLACK, WHITE);
    inkplate.setTextWrap(true);

    // Initialize At Over SPI library.
    if (!WiFi.init())
    {
        inkplate.println("ESP32-C3 initializaiton Failed! Code stopped.");
        inkplate.partialUpdate(true);

        while (1)
        {
            delay(100);
        }
    }
    inkplate.println("ESP32 Initialization OK!");
    inkplate.partialUpdate(true);

    // Set to Access Point to change the MAC Address.
    if (!WiFi.setMode(INKPLATE_WIFI_MODE_AP))
    {
        inkplate.println("AP mode failed!");
        inkplate.partialUpdate(true);

        while (1)
        {
            delay(100);
        }
    }

    // Print out ESP32 MAC address.
    inkplate.print("ESP32 MAC Address: ");
    inkplate.println(WiFi.macAddress());
    inkplate.partialUpdate(true);

    // Change MAC address to something else.
    if(!WiFi.macAddress("1a:bb:cc:01:23:45"))
    {
        inkplate.println("MAC address Change failed!");
        inkplate.partialUpdate(true);
    }
    else
    {
        inkplate.print("New MAC address: ");
        inkplate.println(WiFi.macAddress());
        inkplate.partialUpdate(true);
    }

    // Set it back to the station mode.
    if (!WiFi.setMode(INKPLATE_WIFI_MODE_STA))
    {
        inkplate.println("STA mode failed!");
        inkplate.partialUpdate(true);

        while (1)
        {
            delay(100);
        }
    }

    inkplate.println("WiFi network scan:");
    inkplate.partialUpdate(true);
    // Scan the WiFi networks.
    int foundNetworks = WiFi.scanNetworks();
    if (foundNetworks != 0)
    {
        inkplate.print("Found networks: ");
        inkplate.println(foundNetworks, DEC);
        
        for (int i = 0; i < foundNetworks; i++)
        {
            inkplate.print(i + 1, DEC);
            inkplate.print(". RSSI: ");
            inkplate.print(WiFi.rssi(i), DEC);
            inkplate.print(' ');
            inkplate.print(WiFi.auth(i)?'*':' ');
            inkplate.println(WiFi.ssid(i));
        }
    }
    else
    {
        inkplate.println("No networks found.");
    }
    inkplate.partialUpdate(true);

    inkplate.print("Connecting to the wifi...");
    inkplate.partialUpdate(true);
    WiFi.begin("Soldered", "dasduino");
    while (!WiFi.connected())
    {
        inkplate.print('.');
        inkplate.partialUpdate(true);
        delay(1000);
    }
    inkplate.println("connected!");
    inkplate.partialUpdate(true);

    inkplate.print("Local IP: ");
    inkplate.println(WiFi.localIP());

    inkplate.print("Gateway: ");
    inkplate.println(WiFi.gatewayIP());

    inkplate.print("Subnet mask: ");
    inkplate.println(WiFi.subnetMask());
    inkplate.partialUpdate(true);

    for (int i = 0; i < 3; i++)
    {
        inkplate.print("DNS ");
        inkplate.print(i, DEC);
        inkplate.print(": ");
        inkplate.println(WiFi.dns(i));
    }
    inkplate.partialUpdate(true);

    inkplate.println("Trying to open a text file from the internet...");
    inkplate.partialUpdate(true);

    WiFiClient myClient;
    uint32_t totalSize = 0;

    if (myClient.connect("https://raw.githubusercontent.com/BornaBiro/ESP32-C3-SPI-AT-Commands/main/lorem_ipsum_long.txt"))
    //if (myClient.connect("https://raw.githubusercontent.com/BornaBiro/ESP32-C3-SPI-AT-Commands/main/lorem_ipsum.txt"))
    {
        inkplate.println("Connected!");
        inkplate.partialUpdate(true);
        delay(1000);
        inkplate.clearDisplay();
        inkplate.setCursor(0, 0);
        inkplate.display();

        while (myClient.available())
        {
            if (myClient.available() > 0)
            {
                char myBuffer[2000];
                int n = myClient.read(myBuffer, sizeof(myBuffer));
                totalSize += n;

                myBuffer[n] = 0;
                
                inkplate.print(myBuffer);
                //Serial.print(myBuffer);
            }
        }
        inkplate.partialUpdate(true);
    }
    else
    {
        inkplate.println("Failed to get the file");
        inkplate.partialUpdate(true);
    }

    Serial.print("\n\n\n\nTotal size in bytes: ");
    Serial.println(totalSize, DEC);

    Serial.println("Done!");

    //wifi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
}

void loop()
{

}

