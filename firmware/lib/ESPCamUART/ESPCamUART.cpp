#include "ESPCamUART.h"

// specifies the time of the sun rise starting from 12PM at the first day of each month
// Brussels timezone (UTC+1) WITHOUT daylight savings!!!
static uint32_t sunRiseTable[12] = {
    8.8 * 60 * 60,   // January
    8.367 * 60 * 60, // February
    7.5 * 60 * 60,   // March
    6.35 * 60 * 60,  // April
    5.317 * 60 * 60, //  May
    4.617 * 60 * 60, // June
    4.583 * 60 * 60, // July
    5.183 * 60 * 60, // Aug
    5.983 * 60 * 60, // sep
    6.75 * 60 * 60,  // Oct
    7.617 * 60 * 60, // nov
    8.433 * 60 * 60, // Dec
};

void ESPCamUART::setup()
{
    Serial.println("Waking ESPCAM.");

    pinMode(pin, OUTPUT);
    gpio_hold_dis(pin);
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);

    camSerial->begin(9600, SERIAL_8N1, -1, pin, true);

    Serial.println("Configuring ESPCAM UART.");
}

void ESPCamUART::startMeasurement()
{
    // send the get status message
    // camSerial->write(ESPCamUARTCommand::GET_STATUS);
    // write current time
    delay(100);
    time_t ctime = time(nullptr);
    camSerial->write(ESPCamCodes::SET_TIME);
    camSerial->write((uint8_t*)&ctime, sizeof(ctime));

    Serial.println("Starting measurement, writing time");
    Serial.println(ctime);
}

SensorValue ESPCamUART::getMeasurement()
{
    // uint8_t status;
    Serial.println("[ESPCAM] Taking picture");
    camSerial->write(ESPCamCodes::TAKE_PICTURE);
    Serial.println("[ESPCAM] ESPCamUARTCommand sent");
    camSerial->flush();
    Serial.println("[ESPCAM] Flushing");
    camSerial->end();
    Serial.println("[ESPCAM] End");
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    gpio_hold_en(pin);

    return SensorValue(getID(), 0);
}

void ESPCamUART::updateNextSampleTime(uint32_t sampleInterval)
{
    Serial.println("[ESPCAM] ADAPTIVE adaptive interval called.");
    // extract month and day from current time
    time_t t{nextSampleTime + 24 * 60 * 60};
    tm* day{gmtime(&t)};

    // interpolate sunrise time
    uint32_t pointA = sunRiseTable[day->tm_mon];
    uint32_t pointB = sunRiseTable[(day->tm_mon + 1) % 12];
    uint32_t point = pointB * (day->tm_mday / 31) + pointA * ((31 - day->tm_mday) / 31);

    // calculate target sample time
    uint32_t target = ((t / 60 / 60 / 24) * 60 * 60 * 24) + point + 2 * 60 * 60; // + 2 hours (best lighting conditions after sunrise)
    while ((target - nextSampleTime) > (sampleInterval / 2))
        nextSampleTime += sampleInterval;
}
