#include "ESPCam.h"


// specifies the time of the sun rise starting from 12PM at the first day of each month
// Brussels timezone (UTC+1) WITHOUT daylight savings!!!
static double sunRiseTable[12] = {
    8.8*60*60,   // January
    8.367*60*60, // February 
    7.5*60*60, // March
    6.35*60*60, // April
    5.317*60*60, //  May
    4.617*60*60, // June
    4.583*60*60, // July
    5.183*60*60,   // Aug
    5.983*60*60,   // sep
    6.75*60*60, // Oct
    7.617*60*60, // nov
    8.433*60*60, // Dec 
};


ESPCam::ESPCam(uint8_t triggerPin, uint8_t statusPin) {
    this->triggerPin = triggerPin;
    this->statusPin = statusPin;

    pinMode(triggerPin, OUTPUT);
    pinMode(statusPin, INPUT_PULLDOWN);

    digitalWrite(triggerPin, LOW);
    //gpio_hold_en((gpio_num_t) triggerPin);
    //gpio_deep_sleep_hold_en();
}

void ESPCam::setup(void) {
    pinMode(triggerPin, OUTPUT);
    digitalWrite(this->triggerPin, HIGH);
    gpio_hold_dis((gpio_num_t) this->triggerPin);
    
    //delay(500);
    //digitalWrite(this->triggerPin, LOW);
    //gpio_hold_en((gpio_num_t) this->triggerPin);

}

void ESPCam::start_measurement(){

}

uint8_t ESPCam::read_measurement(float* data, uint8_t length) {
    digitalWrite(this->triggerPin, LOW);
    gpio_hold_en((gpio_num_t) this->triggerPin);

    delay(2);

    if(digitalRead(this->statusPin) == HIGH){
        *data = 1.0;
    } else {
        *data = 0.0;
    }


    
    return 1;
}

ESPCam::~ESPCam() {

}


uint32_t ESPCam::adaptive_sample_interval_update(time_t ctime){
    Serial.println("ADAPTIVE adaptive interval called.");
    // extract month and day from current time
    struct tm result;
    gmtime_r(&ctime, &result);

    // interpolate sunrise time
    double pointA = sunRiseTable[result.tm_mon];
    double pointB = sunRiseTable[(result.tm_mon+1) % 12];
    double point = pointB*result.tm_mday / 31 + pointA * (31-result.tm_mday) / 31;
    
    // calculate sample interval
    time_t ttime = (ctime / 60 / 60 / 24) * 60 * 60 * 24;
    ttime += (time_t) point; 
    ttime += 2*60*60; // + 2 hours (best lighting conditions after sunrise)

    if(ttime <= ctime){
        ttime += 24*60*60;
    }

    // return sample interval: target time - current time
    return ttime - ctime;
}
