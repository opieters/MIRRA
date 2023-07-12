#ifndef __ESP_CAM_UART_H__
#define __ESP_CAM_UART_H__

#include "Sensor.h"
#include <Arduino.h>
#include <AsyncAPDS9306.h>
#include <ESPCamCodes.h>
#include <HardwareSerial.h>

class ESPCamUART : public Sensor
{
public:
    ESPCamUART(HardwareSerial* serial, gpio_num_t pin);

    void setup(void);

    void start_measurement(void);

    uint8_t read_measurement(float* meas, uint8_t n_meas);

    void stop_measurement(void){};

    ~ESPCamUART();

    const uint8_t getID() { return 64; };

    uint32_t adaptive_sample_interval_update(time_t ctime);

private:
    HardwareSerial* serial;
    const gpio_num_t pin;
};
#endif