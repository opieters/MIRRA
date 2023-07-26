#include "ESPCamUART.h"

// specifies the time of the sun rise starting from 12PM at the first day of each month
// Brussels timezone (UTC+1) WITHOUT daylight savings!!!
static uint32_t sunRiseTable[12] = {
    8 * 60 + 48, // January
    8 * 60 + 22, // February
    7 * 60 + 30, // March
    6 * 60 + 21, // April
    5 * 60 + 19, // May
    4 * 60 + 37, // June
    4 * 60 + 35, // July
    5 * 60 + 11, // August
    5 * 60 + 59, // September
    6 * 60 + 45, // October
    7 * 60 + 37, // November
    8 * 60 + 26, // December
};

void ESPCamUART::startMeasurement()
{
    Serial.println("[ESPCAM] Waking....");

    pinMode(pin, OUTPUT);
    gpio_hold_dis(pin);
    digitalWrite(pin, HIGH);
    camSerial->begin(9600, SERIAL_8N1, -1, pin, true);
    delay(100);
    time_t ctime{time(nullptr)};
    camSerial->write(ESPCamCodes::SET_TIME);
    camSerial->write((uint8_t*)&ctime, sizeof(ctime));
    camSerial->flush();

    Serial.printf("[ESPCAM] Starting measurement, writing time: %i\n", ctime);
}

SensorValue ESPCamUART::getMeasurement()
{
    Serial.println("[ESPCAM] Taking picture");
    camSerial->write(ESPCamCodes::TAKE_PICTURE);
    camSerial->flush();
    Serial.println("[ESPCAM] ESPCamUARTCommand sent");
    camSerial->end();
    Serial.println("[ESPCAM] End");
    digitalWrite(pin, LOW);
    gpio_hold_en(pin);

    return SensorValue(getID(), 0);
}

void ESPCamUART::updateNextSampleTime(uint32_t sampleInterval)
{
    nextSampleTime += 3 * sampleInterval;
    return;
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
