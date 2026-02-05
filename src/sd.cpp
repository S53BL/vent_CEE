// sd.cpp - SD card module implementation
#include "sd.h"
#include "config.h"
#include <SPI.h>
#include <SdFat.h>

// SdFat object
SdFat SD;

bool initSD() {
    // Initialize SPI for SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, SPI_FULL_SPEED)) {
        Serial.println("SD: Card not present");
        return false;
    }

    // Get card size
    uint64_t cardSize = (uint64_t)SD.card()->sectorCount() * 512; // Size in bytes
    if (cardSize == 0) {
        Serial.println("SD: Card not present");
        return false;
    }

    // Convert to MB
    uint64_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.printf("SD: Card present, size: %llu MB\n", cardSizeMB);

    return true;
}
