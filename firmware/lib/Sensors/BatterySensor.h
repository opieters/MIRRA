#ifndef __BATTERY_SENSOR_H__
#define __BATTERY_SENSOR_H__

#include "Sensor.h"
#include <Arduino.h>

#define BATTERY_KEY 1

class BatterySensor final : public Sensor
{
private:
    uint8_t pin, enablePin;

public:
    BatterySensor(uint8_t pin, uint8_t enablePin) : pin{pin}, enablePin{enablePin} {};
    void setup();
    void startMeasurement();
    SensorValue getMeasurement();
    uint8_t getID() const { return BATTERY_KEY; };
};
#endif