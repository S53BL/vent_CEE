#ifndef SENS_H
#define SENS_H

#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BME280.h>
#include "globals.h"
#include "logging.h"

void initI2CBus();
void initSensors();
void readSensors();
bool checkI2CDevice(uint8_t address);
void resetI2CBus();
void performPeriodicSensorCheck();
void performPeriodicI2CReset();

void setupInputs();
void readInputs();

int determineCycleMode();

#endif // SENS_H
