#ifndef __TEMP_RH_SENSOR_H__
#define __TEMP_RH_SENSOR_H__

#include "Sensor.h"
#include <Arduino.h>
#include <SHTSensor.h>

#define TEMP_SHT_KEY 12
#define HUMI_SHT_KEY 13

class TempSHTSensor final : public Sensor
{
private:
    SHTSensor baseSensor;

public:
    TempSHTSensor() : baseSensor{SHTSensor(SHTSensor::SHT3X)} {};
    void setup();
    void startMeasurement();
    SensorValue getMeasurement();
    uint8_t getID() const { return TEMP_SHT_KEY; };

    const SHTSensor& getBase() const { return baseSensor; };
};

class HumiSHTSensor final : public Sensor
{
private:
    const SHTSensor& baseSensor;

public:
    HumiSHTSensor(const TempSHTSensor& base) : baseSensor{base.getBase()} {};
    SensorValue getMeasurement();
    uint8_t getID() const { return HUMI_SHT_KEY; };
};
#endif