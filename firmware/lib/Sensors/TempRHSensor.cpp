#include "TempRHSensor.h"
#include "Wire.h"

void TempRHSensor::setup(void)
{
    Serial.println("SHT setup.");
    if (baseSensor.init())
    {
        Serial.println("SHT init OK.");
    }
}

void TempRHSensor::readMeasurement()
{
    uint8_t i = 10;
    baseSensor.readSample();
    this->measurement[0] = baseSensor.getTemperature();
    this->measurement[1] = baseSensor.getHumidity();
}
