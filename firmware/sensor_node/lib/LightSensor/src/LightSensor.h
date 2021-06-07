#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

#include <Arduino.h>
#include "Sensor.h"
#include <AsyncAPDS9306.h>

class LightSensor : public Sensor {
    public:
        LightSensor();

        void setup(void);

        void start_measurement(void);

        uint8_t read_measurement(float* meas, uint8_t n_meas);

        void stop_measurement(void) {};

        ~LightSensor();

        const uint8_t getID() { return 22; };

    private:
        AsyncAPDS9306 baseSensor;
};
#endif