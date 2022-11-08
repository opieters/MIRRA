#ifndef __ESP_CAM_UART_H__
#define __ESP_CAM_UART_H__

#include <Arduino.h>
#include "Sensor.h"
#include <AsyncAPDS9306.h>
#include <Software/OWI.h>
#include <HardwareSerial.h>


class ESPCamUART : public Sensor {
    public:
        ESPCamUART(HardwareSerial* serial, gpio_num_t pin, gpio_num_t stat1_pin, gpio_num_t stat2_pin);

        void setup(void);

        void start_measurement(void);

        uint8_t read_measurement(float* meas, uint8_t n_meas);

        void stop_measurement(void) {};

        ~ESPCamUART();

        const uint8_t getID() { return 64; };

        //uint32_t adaptive_sample_interval_update(time_t ctime);

        enum ESPCamUARTCommand {
            SET_TIME = 11,
            GET_TIME = 22,
            TAKE_PICTURE = 33,
            GET_STATUS = 55,
            ENABLE_SLEEP = 44,
        };

    private:
        HardwareSerial* serial;
        const gpio_num_t pin;
        const gpio_num_t stat1_pin, stat2_pin;

};
#endif