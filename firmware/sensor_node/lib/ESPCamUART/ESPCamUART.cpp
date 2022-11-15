#include "ESPCamUART.h"
#include "PCF2129_RTC.h"

extern PCF2129_RTC rtc;

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


ESPCamUART::ESPCamUART(HardwareSerial* serial, const gpio_num_t pin, gpio_num_t a1, gpio_num_t a2) : serial(serial), pin(pin), stat1_pin(a1), stat2_pin(a2) {
}

void ESPCamUART::setup(void) {

    Serial.println("Waking ESPCAM.");
    

    pinMode(pin, OUTPUT);
    gpio_hold_dis(pin);
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);

    serial->begin(9600, SERIAL_8N1, -1, pin, true);

    pinMode(stat1_pin, INPUT);
    pinMode(stat2_pin, INPUT);

    Serial.println("Configuring ESPCAM UART.");
}

void ESPCamUART::start_measurement(){
    // send the get status message
    serial->write(ESPCamUARTCommand::GET_STATUS);
    // write current time
    delay(100);
    time_t ctime = rtc.read_time_epoch();
    serial->write(ESPCamUARTCommand::SET_TIME);
    serial->write((uint8_t*) &ctime, sizeof(ctime));

    Serial.println("Starting measurement, writing time");
    Serial.println(ctime);
}

uint8_t ESPCamUART::read_measurement(float* data, uint8_t length) {
    //uint8_t status;
    digitalRead(stat1_pin)==HIGH ? data[0] = 1.0 : data[0] = 0.0;
    digitalRead(stat2_pin)==HIGH ? data[1] = 1.0 : data[1] = 0.0;

    serial->write(ESPCamUARTCommand::TAKE_PICTURE);
    serial->flush();
    serial->end();
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    gpio_hold_en(pin);
    
    return 2;
}

ESPCamUART::~ESPCamUART() {

}


uint32_t ESPCamUART::adaptive_sample_interval_update(time_t ctime){
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
