#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

#include <Arduino.h>
#include "Sensor.h"
#include <AsyncAPDS9306.h>

#define LIGHT_KEY 22

class LightSensor : public Sensor
{
private:
    AsyncAPDS9306 baseSensor;
    float measurement;

public:
    LightSensor() : baseSensor{AsyncAPDS9306()} {}
    void setup(){};
    void startMeasurement(void);
    void readMeasurement();
    void stopMeasurement(){};
    SensorValue getValue() { return SensorValue(getID(), this->measurement); };
    const uint8_t getID() { return LIGHT_KEY; };
};
#endif