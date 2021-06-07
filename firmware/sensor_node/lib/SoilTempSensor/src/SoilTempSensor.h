#ifndef __SOIL_TEMPERATURE_SENSOR_H__
#define __SOIL_TEMPERATURE_SENSOR_H__

#include <DallasTemperature.h>
#include <OneWire.h>

#include "Sensor.h"

class SoilTemperatureSensor : public Sensor
{
  private:
    uint8_t busIndex;              // The bus index of the sensor
    uint8_t pin;                   // pin number of 1W interface

    DeviceAddress longWireThermometer, shortWireThermometer;

    void printTemperature(DeviceAddress deviceAddress);
  public:
    /**
     * Constructor
     *
     * @param pin   The pin on which the sensor is connected
     *                
     * @param index The onewire bus index of the sensor
     */
    SoilTemperatureSensor(uint8_t pin, uint8_t busIndex);

    void setup() {};

    /**
     * Returns the temperature in °C, resolution = 0.01 °C
     */
    void start_measurement() {};
    void stop_measurement() {};

    uint8_t read_measurement(float* meas, uint8_t n_meas);

    const uint8_t getID() { return 4; };

};

#endif
