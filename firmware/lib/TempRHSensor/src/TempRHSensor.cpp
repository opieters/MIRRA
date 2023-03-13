#include "TempRHSensor.h"
#include "Wire.h"

TempRHSensor::TempRHSensor() {
    this->baseSensor = SHTSensor(SHTSensor::SHT3X);
}

void TempRHSensor::setup(void) {
    Serial.println("SHT setup.");
    if(baseSensor.init()){
        Serial.println("SHT init OK.");
    }
    
}

void TempRHSensor::start_measurement(){
}

uint8_t TempRHSensor::read_measurement(float* data, uint8_t length) {
    uint8_t i = 10;

    if(length < 2){
        return 0;
    }

    while(i--){
        delay(100);
        if(baseSensor.readSample()){
            data[0] = baseSensor.getTemperature();
            data[1] = baseSensor.getHumidity();
            return 2;
            break;
        }
    }

    return 0;
}
