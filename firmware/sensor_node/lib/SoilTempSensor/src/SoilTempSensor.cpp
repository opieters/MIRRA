#include <DallasTemperature.h>
#include "SoilTempSensor.h"

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void SoilTemperatureSensor::printTemperature(DeviceAddress deviceAddress)
{
    float temperature;
    read_measurement(&temperature, 1);
    Serial.print("    Temp C:");
    Serial.println(temperature);
}


SoilTemperatureSensor::SoilTemperatureSensor(uint8_t pin, uint8_t busIndex)
{
    this->busIndex = busIndex;
    this->pin = pin;
};

uint8_t SoilTemperatureSensor::read_measurement(float* data, uint8_t length)
{
    if(length < 1){
        return 0;
    }

    auto onewire = OneWire(pin);
    auto tempSensor = DallasTemperature(&onewire);
    tempSensor.begin();
    delay(1000);

    tempSensor.getAddress(longWireThermometer, 0);
    tempSensor.getAddress(shortWireThermometer, 1);

    tempSensor.requestTemperaturesByAddress(longWireThermometer);

    // print the device information:
    //Serial.println("longwire thermometer:");
    //printTemperature(longWireThermometer);

    //Serial.println("\nshortwire thermometer:");
    //printTemperature(shortWireThermometer);

    delay(1000);
    data[0] = tempSensor.getTempCByIndex(this->busIndex);

    return 1;
}