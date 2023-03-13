#include "LightSensor.h"

LightSensor::LightSensor() {
    this->baseSensor = AsyncAPDS9306();
}

void LightSensor::setup(void) {
    baseSensor.begin(APDS9306_ALS_GAIN_1, APDS9306_ALS_MEAS_RES_16BIT_25MS);
}

void LightSensor::start_measurement(){
    baseSensor.startLuminosityMeasurement();
}

uint8_t LightSensor::read_measurement(float* data, uint8_t length) {
    if(length < 1){
        return 0;
    }

    while(!baseSensor.isMeasurementReady());

    AsyncAPDS9306Data sensorData = baseSensor.getLuminosityMeasurement();

    *data = float(sensorData.raw);

    return 1;
}

LightSensor::~LightSensor() {
    
}
