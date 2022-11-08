#ifndef __ESP_CAM_OW_H__
#define __ESP_CAM_OW_H__

#include <Arduino.h>
#include "Sensor.h"
#include <AsyncAPDS9306.h>
#include <Software/OWI.h>

class ESPCamOW : public Sensor {
    public:
        ESPCamOW(Software::OWI owi, uint8_t triggerPin);

        void setup(void);

        void start_measurement(void);

        uint8_t read_measurement(float* meas, uint8_t n_meas);

        void stop_measurement(void) {};

        ~ESPCamOW();

        const uint8_t getID() { return 64; };

        uint32_t adaptive_sample_interval_update(time_t ctime);

    private:
        Software::OWI owi;
        const uint8_t triggerPin;
};
#endif