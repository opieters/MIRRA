#include "LightSensor.h"

void LightSensor::startMeasurement()
{
    baseSensor.begin(APDS9306_ALS_GAIN_1, APDS9306_ALS_MEAS_RES_16BIT_25MS);
    baseSensor.startLuminosityMeasurement();
}

void LightSensor::readMeasurement()
{
    while (!baseSensor.isMeasurementReady())
        ;
    this->measurement = static_cast<float>(baseSensor.getLuminosityMeasurement().raw);
}