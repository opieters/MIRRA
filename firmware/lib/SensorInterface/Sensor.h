#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <time.h>

class SensorValue
{
private:
    uint16_t tag;
    float value;

public:
    SensorValue() : tag{0}, value{0} {};
    SensorValue(unsigned int type_tag, unsigned int instance_tag, float value) : tag{(uint16_t)(((type_tag & 0xFFF) << 4) | (instance_tag & 0xF))}, value{value} {};
    SensorValue(uint16_t tag, float value) : tag{tag}, value{value} {};
} __attribute__((packed));

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
    virtual void readMeasurement() = 0;
    virtual void stopMeasurement() = 0;
    virtual SensorValue getValue() = 0;
    virtual const uint8_t getID() = 0;
    virtual uint32_t adaptive_sample_interval_update(time_t ctime);
    virtual void set_sample_interval(uint32_t sample_interval) { this->sample_interval = sample_interval; };

private:
    uint32_t sample_interval = 5 * 60; // sample every hour by default
};

#endif
