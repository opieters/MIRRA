#ifndef __SOIL_TEMPERATURE_SENSOR_H__
#define __SOIL_TEMPERATURE_SENSOR_H__

#include <DallasTemperature.h>
#include <OneWire.h>

#include "Sensor.h"

#define SOIL_TEMPERATURE_KEY 4;
class SoilTemperatureSensor final : public Sensor
{
private:
    uint8_t busIndex; // The bus index of the sensor
    uint8_t pin;      // pin number of 1W interface
    OneWire wire;
    DallasTemperature dallas;

public:
    /**
     * Constructor
     *
     * @param pin   The pin on which the sensor is connected
     *
     * @param busIndex The onewire bus index of the sensor
     */
    SoilTemperatureSensor(uint8_t pin, uint8_t busIndex) : pin{pin}, busIndex{busIndex}, wire{OneWire(pin)}, dallas{&wire} {}
    void setup(){};
    void startMeasurement();
    SensorValue getMeasurement();
    uint8_t getID() const { return SOIL_TEMPERATURE_KEY; };
};

#endif
