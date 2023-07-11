#ifndef __RANDOM_SENSOR_H__
#define __RANDOM_SENSOR_H__

#include "Sensor.h"

#define RANDOM_KEY 100;
class RandomSensor : public Sensor
{
private:
    uint8_t measurement;

public:
    RandomSensor(uint32_t seed) { srand(seed); }
    void setup(){};
    void startMeasurement(){};
    void readMeasurement() { this->measurement = rand() % 101; };
    void stopMeasurement(){};
    SensorValue getValue() { return SensorValue(getID(), static_cast<float>(this->measurement)); };
    uint8_t getID() const { return RANDOM_KEY; }
};
#endif