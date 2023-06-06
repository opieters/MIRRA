#include <DallasTemperature.h>
#include "SoilTempSensor.h"

void SoilTemperatureSensor::printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void SoilTemperatureSensor::startMeasurement()
{
    dallas.begin();
}

void SoilTemperatureSensor::readMeasurement()
{

    dallas.getAddress(longWireThermometer, 0);
    dallas.getAddress(shortWireThermometer, 1);

    dallas.requestTemperaturesByAddress(longWireThermometer);
    this->measurement = dallas.getTempCByIndex(this->busIndex);
}