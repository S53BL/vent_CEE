#ifndef SENS_H
#define SENS_H

#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BME280.h>
#include "globals.h"
#include "logging.h"

void initI2CBus(bool force = false);
void initSensors();
void readSensors();
bool checkI2CDevice(uint8_t address);
bool resetI2CBus();
void performPeriodicSensorCheck();
void performSmartI2CMaintenance();

void setupInputs();
void readInputs();

int determineCycleMode(float int_temp, float int_hum, uint8_t sensor_err_flag);

#endif // SENS_H
