#ifndef __ESP_CAM_UART_H__
#define __ESP_CAM_UART_H__

#include "Sensor.h"
#include <Arduino.h>
#include <ESPCamCodes.h>
#include <HardwareSerial.h>

#define CAM_KEY 64

class ESPCamUART final : public Sensor
{
private:
    HardwareSerial* camSerial;
    const gpio_num_t pin;

public:
    ESPCamUART(HardwareSerial* camSerial, gpio_num_t pin) : camSerial{camSerial}, pin{pin} {};
    void setup();
    void startMeasurement();
    SensorValue getMeasurement();
    uint8_t getID() const { return CAM_KEY; };

    void updateNextSampleTime(uint32_t sampleInterval);
};
#endif