#include "ESPCam.h"


// specifies the time of the sun rise starting from 12PM at the first day of each month
static double sunRiseTable[12] = {
    9*60*60,   // January
    8.5*60*60, // February 
    7.5*60*60, // March
    6.5*60*60, // April
    5.5*60*60, //  May
    4.0*60*60, // June
    5.5*60*60, // July
    6*60*60,   // Aug
    7*60*60,   // sep
    7.8*60*60, // Oct
    8.5*60*60, // nov
    9.3*60*60, // Dec 
};

ESPCam::ESPCam(uint8_t triggerPin, uint8_t statusPin) {
    this->triggerPin = triggerPin;
    this->statusPin = statusPin;

    pinMode(triggerPin, OUTPUT);
    pinMode(statusPin, INPUT_PULLDOWN);

    digitalWrite(triggerPin, LOW);
}

void ESPCam::setup(void) {
    digitalWrite(this->triggerPin, HIGH);
}

void ESPCam::start_measurement(){

}

uint8_t ESPCam::read_measurement(float* data, uint8_t length) {
    // TODO: add if condition that checks image sensor status
    
    digitalWrite(this->triggerPin, LOW);

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
    // extract month and day from current time
    struct tm result;
    gmtime_r(&ctime, &result);

    // interpolate sunrise time
    double pointA = sunRiseTable[result.tm_mon];
    double pointB = sunRiseTable[(result.tm_mon+1) % 12];
    double point = pointA*result.tm_mday / 31 + pointB * (31-result.tm_mday) / 31;
    
    // calculate sample interval
    time_t ttime = (ctime / 60 / 60 / 24 + 1) * 60 * 60 * 24;
    ttime += (time_t) point; 

    return ttime - ctime;
}