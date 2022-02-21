#ifndef __ESP_CAM_H__
#define __ESP_CAM_H__

#include <Arduino.h>
#include "Sensor.h"
#include <AsyncAPDS9306.h>

class ESPCam : public Sensor {
    public:
        ESPCam(uint8_t triggerPin, uint8_t statusPin);

        void setup(void);

        void start_measurement(void);

        uint8_t read_measurement(float* meas, uint8_t n_meas);

        void stop_measurement(void) {};

        ~ESPCam();

        const uint8_t getID() { return 64; };

        uint32_t adaptive_sample_interval_update(time_t ctime);


    private:
        uint8_t triggerPin, statusPin;
};
#endif