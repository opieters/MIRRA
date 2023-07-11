#ifndef __TEMP_RH_SENSOR_H__
#define __TEMP_RH_SENSOR_H__

#include "Sensor.h"
#include <Arduino.h>
#include <SHTSensor.h>

#define TEMP_RH_KEY 12

class TempRHSensor : public Sensor
{
private:
    SHTSensor baseSensor;
    float measurement[2];

public:
    TempRHSensor() : baseSensor{SHTSensor(SHTSensor::SHT3X)} {};
    void setup();
    void startMeasurement(){};
    void readMeasurement();
    void stopMeasurement(){};
    SensorValue getValue() { return SensorValue(getID(), static_cast<float>(this->measurement[0])); };
    uint8_t getID() const { return TEMP_RH_KEY; };
};
#endif