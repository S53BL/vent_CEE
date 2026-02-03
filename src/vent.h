// vent.h - Fan control declarations for CEE

#ifndef VENT_H
#define VENT_H

#include <Arduino.h>

void setupVent();
void controlFans();
void controlWC();
void controlUtility();
void controlBathroom();
void controlLivingRoom();
void calculatePower();

#endif // VENT_H
