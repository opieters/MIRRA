#include "SoilMoistureSensor.h"

SoilMoistureSensor::SoilMoistureSensor(uint8_t pin)
{
    this->pin = pin;
    pinMode(pin, INPUT);
}

void SoilMoistureSensor::start_measurement(void){
}

uint8_t SoilMoistureSensor::read_measurement(float* meas, uint8_t n_meas){
    if(n_meas < 1){
        return 0;
    }

    uint16_t value = analogRead(pin);

    meas[0] = (float) value;

    return 1;
}

void SoilMoistureSensor::stop_measurement(void){
}

const uint8_t getID(){ 
    return SOIL_MOISTURE_KEY; 
}