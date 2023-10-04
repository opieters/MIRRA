#include "ESPCamUART.h"

// specifies the time of the sun rise starting from 12PM at the first day of each month
// UTC WITHOUT daylight savings!!!
static uint32_t sunRiseTable[12] = {
    7 * 60 * 60 + 48 * 60, // January
    7 * 60 * 60 + 22 * 60, // February
    6 * 60 * 60 + 30 * 60, // March
    5 * 60 * 60 + 21 * 60, // April
    4 * 60 * 60 + 19 * 60, // May
    3 * 60 * 60 + 37 * 60, // June
    3 * 60 * 60 + 35 * 60, // July
    4 * 60 * 60 + 11 * 60, // August
    4 * 60 * 60 + 59 * 60, // September
    5 * 60 * 60 + 45 * 60, // October
    6 * 60 * 60 + 37 * 60, // November
    7 * 60 * 60 + 26 * 60, // December
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
    uint32_t cTime{static_cast<uint32_t>(time(nullptr))};
    uint32_t target{cTime};
    while (nextSampleTime <= cTime)
    {
        // extract month and day from current target
        tm day;
        gmtime_r(reinterpret_cast<time_t*>(&target), &day);

        // interpolate sunrise time
        uint32_t pointA = sunRiseTable[day.tm_mon];
        uint32_t pointB = sunRiseTable[(day.tm_mon + 1) % 12];
        uint32_t point = (pointB * day.tm_mday) / 31 + (pointA * (31 - day.tm_mday)) / 31;

        // calculate target sample time
        target = ((target / 60 / 60 / 24) * 60 * 60 * 24) + point + 2 * 60 * 60; // + 2 hours (best lighting conditions after sunrise)
        nextSampleTime = (target / sampleInterval) * sampleInterval + (nextSampleTime % sampleInterval);
        target += 24 * 60 * 60;
    }
}
