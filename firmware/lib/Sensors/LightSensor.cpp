#include "LightSensor.h"

void LightSensor::startMeasurement()
{
    baseSensor.begin(APDS9306_ALS_GAIN_1, APDS9306_ALS_MEAS_RES_16BIT_25MS);
    baseSensor.startLuminosityMeasurement();
}

SensorValue LightSensor::getMeasurement()
{
    while (!baseSensor.isMeasurementReady())
        ;
    return SensorValue(getID(), static_cast<float>(baseSensor.getLuminosityMeasurement().raw));
}