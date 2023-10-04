#include <BatterySensor.h>

void BatterySensor::setup()
{
    pinMode(enablePin, OUTPUT);
    pinMode(pin, INPUT);
}
void BatterySensor::startMeasurement() { digitalWrite(enablePin, HIGH); }
SensorValue BatterySensor::getMeasurement()
{
    SensorValue value{getID(), 2 * static_cast<float>(analogReadMilliVolts(pin)) / 1000}; // times two because of voltage divider (see schematic)
    digitalWrite(enablePin, LOW);
    return value;
}