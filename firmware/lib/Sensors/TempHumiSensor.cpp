#include "Wire.h"
#include <TempHumiSensor.h>

void TempSHTSensor::setup(void)
{
    Serial.println("SHT setup.");
    if (baseSensor.init())
    {
        Serial.println("SHT init OK.");
    }
}
void TempSHTSensor::startMeasurement() { baseSensor.readSample(); }

SensorValue TempSHTSensor::getMeasurement() { return SensorValue(getID(), baseSensor.getTemperature()); }

SensorValue HumiSHTSensor::getMeasurement() { return SensorValue(getID(), baseSensor.getHumidity()); }
