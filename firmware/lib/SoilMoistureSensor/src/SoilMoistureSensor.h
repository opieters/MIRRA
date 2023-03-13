#ifndef __SOIL_MOISTURE_SENSOR_H__
#define __SOIL_MOISTURE_SENSOR_H__

#include <Arduino.h>

#include "Sensor.h"

#define SOIL_MOISTURE_KEY 3

class SoilMoistureSensor : public Sensor
{
  public:
    /**
     * Constructor
     *
     * @param pin   The pin on which the sensor is connected
     */
    SoilMoistureSensor(uint8_t pin);

    void setup() {};

    /**
     * Returns the soil moisture value (12 bit resolution)
     */
    uint8_t read_measurement(float* data, uint8_t length);

    void start_measurement(void);
    void stop_measurement(void);

    const uint8_t getID() { return 3; };

  private:
    uint8_t pin;
    uint8_t powerPin;
};

#endif
