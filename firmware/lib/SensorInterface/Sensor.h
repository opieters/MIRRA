#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <time.h>

struct SensorValue
{
    uint16_t tag{0};
    float value{0};

    SensorValue() = default;
    /// @brief Constructs a SensorValue from the appropriate tags and value.
    /// @param typeTag Type ID of the sensor.
    /// @param instanceTag Instance ID of the sensor.
    /// @param value Concrete value.
    SensorValue(unsigned int typeTag, unsigned int instanceTag, float value) : tag{(uint16_t)(((typeTag & 0xFFF) << 4) | (instanceTag & 0xF))}, value{value} {};
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
    /// @brief Initialises the sensor.
    virtual void setup() {}
    /// @brief Starts the measurement of the sensor.
    virtual void startMeasurement() {}
    // @return The measured value.
    virtual SensorValue getMeasurement() = 0;
    /// @return The sensor's type ID.
    virtual uint8_t getID() const = 0;
    /// @brief Updates the sensor's next sample time according to the sensor-specific algorithm. (usually simply addition)
    /// @param sampleInterval Sample interval with which to update.
    virtual void updateNextSampleTime(uint32_t sampleInterval) { this->nextSampleTime += sampleInterval; };
    /// @brief Forcibly sets the nextSampleTime of the sensor.
    void setNextSampleTime(uint32_t nextSampleTime) { this->nextSampleTime = nextSampleTime; };
    /// @return The next sample time in seconds from UNIX epoch.
    uint32_t getNextSampleTime() const { return nextSampleTime; };
    virtual ~Sensor() = default;

protected:
    uint32_t nextSampleTime = -1;
};

#endif
