#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <time.h>

/**
 * Sensor interface, this interface has to be overriden by all the sensor classes. This class allows to 
 * have a generic way of reading the sensors.
 */
class Sensor {
public:
    /**
     * @return the sensor value as a float
     */
    virtual void setup(void) = 0;

    virtual void start_measurement(void) = 0;

    virtual uint8_t read_measurement(float* meas, uint8_t n_meas) = 0;

    virtual void stop_measurement(void) = 0;

    virtual const uint8_t getID() = 0;

    virtual uint32_t adaptive_sample_interval_update(time_t ctime);

    virtual void set_sample_interval(uint32_t sample_interval) { this->sample_interval = sample_interval; }; 

private:

    uint32_t sample_interval = 60*60; // sample every hour by default

};




#endif
