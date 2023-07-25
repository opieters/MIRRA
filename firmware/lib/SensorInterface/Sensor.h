#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <time.h>

struct SensorValue
{
    uint16_t tag{0};
    float value{0};

    SensorValue() = default;
    SensorValue(unsigned int type_tag, unsigned int instance_tag, float value)
        : tag{(uint16_t)(((type_tag & 0xFFF) << 4) | (instance_tag & 0xF))}, value{value} {};
    SensorValue(uint16_t tag, float value) : tag{tag}, value{value} {};
} __attribute__((packed));

// TODO: Find a way to avoid virtual functions, possibly using CRTP? -> Will make iterating over polymorphic container difficult

/**
 * Sensor interface, this interface has to be overriden by all the sensor classes. This class allows to
 * have a generic way of reading the sensors.
 */
class Sensor
{
public:
    /**
     * @return the sensor value as a float
     */
    virtual void setup() = 0;
    virtual void startMeasurement() = 0;
    virtual SensorValue getMeasurement() = 0;
    virtual uint8_t getID() const = 0;
    virtual void updateNextSampleTime(uint32_t sampleInterval) { this->nextSampleTime += sampleInterval; };
    void setNextSampleTime(uint32_t nextSampleTime) { this->nextSampleTime = nextSampleTime; };
    uint32_t getNextSampleTime() const { return nextSampleTime; };
    virtual ~Sensor() = default;

protected:
    uint32_t nextSampleTime = -1;
};

#endif
