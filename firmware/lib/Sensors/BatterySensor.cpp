#include <BatterySensor.h>

void BatterySensor::setup()
{
    pinMode(enablePin, OUTPUT);
    pinMode(pin, INPUT);
}

void BatterySensor::startMeasurement() { digitalWrite(enablePin, HIGH); }
void BatterySensor::readMeasurement()
{
    this->measurement = 2 * static_cast<float>(analogReadMilliVolts(pin)) / 1000; // times two because of voltage divider (see schematic)
}
void BatterySensor::stopMeasurement() { digitalWrite(enablePin, LOW); }