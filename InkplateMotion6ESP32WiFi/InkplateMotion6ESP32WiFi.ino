// Add an Inkplate Motion Libray to the Sketch.
#include <InkplateMotion.h>

// Include library for AT Commands over SPI.
#include "esp32SpiAt.h"

// Include Typedefs for the AT SPI messages.
#include "WiFiSPITypedef.h"

// Create an Inkplate Motion Object.
Inkplate inkplate;

// Create an object for the AT commands over SPI commands.
AtSpi at;

void setup()
{
    // Setup a Serial communication for debug at 115200 bauds.
    Serial.begin(115200);

    // Print an welcome message (to know if the Inkplate board and STM32 are alive).
    Serial.println("Inkplate Motion Code Started!");

    // Initialize At Over SPI library.
    if (!at.begin())
    {
        Serial.println("ESP32-C3 initializaiton Failed! Code stopped.");

        while (1)
        {
            delay(100);
        }
    }
    Serial.println("ESP32 Initialization OK!");

    char rxBuffer[32768];
    if (!at.sendAtCommand("AT\r\n", rxBuffer, 1000ULL))
    {
        Serial.println("AT Ping Fail!");
    }
    else
    {
        Serial.println("AT Ping Ok, response:\r\n");
        Serial.println(rxBuffer);
    }

    if (!at.sendAtCommand("AT+CWINIT=1\r\n", rxBuffer, 1000ULL))
    {
        Serial.println("AT+CWINIT Fail!");
    }
    else
    {
        Serial.println("AT+CWINIT, response:\r\n");
        Serial.println(rxBuffer);
    }

    if (!at.sendAtCommand("AT+CWMODE=1\r\n", rxBuffer, 1000ULL))
    {
        Serial.println("AT+CWMODE Fail!");
    }
    else
    {
        Serial.println("AT+CWMODE, response:\r\n");
        Serial.println(rxBuffer);
    }

    if (!at.sendAtCommand("AT+CWLAP\r\n", rxBuffer, 10000ULL))
    {
        Serial.println("AT+CWMODE Fail!");
    }
    else
    {
        Serial.println("AT+CWLAP, response:\r\n");
        Serial.println(rxBuffer);
    }


    // Initialize the Inkplate Motion Library.
    //inkplate.begin(INKPLATE_1BW);
}

void loop()
{

}

