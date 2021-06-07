#ifndef __TEMP_RH_SENSOR_H__
#define __TEMP_RH_SENSOR_H__

#include <Arduino.h>
#include "Sensor.h"
#include <SHTSensor.h>

class TempRHSensor : public Sensor {
    public:
        TempRHSensor();

        void setup(void);

        void start_measurement(void);

        uint8_t read_measurement(float* meas, uint8_t n_meas);

        void stop_measurement(void) {};

        const uint8_t getID() { return 12; };

    private:
        SHTSensor baseSensor;
};
#endif