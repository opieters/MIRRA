#ifndef __SOIL_MOISTURE_SENSOR_H__
#define __SOIL_MOISTURE_SENSOR_H__

#include "Sensor.h"
#include <Arduino.h>

#define SOIL_MOISTURE_KEY 3

class SoilMoistureSensor : public Sensor
{
private:
    uint8_t pin;
    uint16_t measurement;

public:
    /**
     * Constructor
     *
     * @param pin   The pin on which the sensor is connected
     */
    SoilMoistureSensor(uint8_t pin) : pin{pin} {};
    void setup(){};
    void startMeasurement() { pinMode(pin, INPUT); };
    void readMeasurement() { this->measurement = analogRead(pin); };
    void stopMeasurement(){};
    SensorValue getValue() { return SensorValue(getID(), static_cast<float>(this->measurement)); };
    uint8_t getID() const { return SOIL_MOISTURE_KEY; };
};

#endif
