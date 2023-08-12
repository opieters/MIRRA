#include "PCF2129_RTC.h"
#include "Arduino.h"

uint8_t PCF2129_RTC::bcdToDec(uint8_t value) { return (uint8_t)(((value >> 4) * 10) + (value & 0x0F)); }

uint8_t PCF2129_RTC::decToBcd(uint8_t value) { return (uint8_t)((value / 10 * 16) + (value % 10)); }

PCF2129_RTC::PCF2129_RTC(uint8_t intPin, uint8_t address) : address{address}, intPin{intPin}
{
    pinMode(intPin, INPUT_PULLUP);
    setSysTime();
}

struct tm PCF2129_RTC::readTime()
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_SECONDS);
    Wire.endTransmission();
    Wire.requestFrom(address, (uint8_t)7);
    while (!Wire.available())
        ;
    int sec{bcdToDec(Wire.read())};
    int min{bcdToDec(Wire.read())};
    int hour{bcdToDec(Wire.read())};
    int mday{bcdToDec(Wire.read())};
    int wday{bcdToDec(Wire.read())};
    int mon{bcdToDec(Wire.read()) - 1};    // (RTC uses 1-12 as months but C time structs use 0-11 as months)
    int year{bcdToDec(Wire.read()) + 100}; // (RTC uses 2000 as reference year but C time structs use 1900 as reference year)

    return tm{sec, min, hour, mday, mon, year, wday};
}

uint32_t PCF2129_RTC::readTimeEpoch()
{
    struct tm now = readTime();
    return mktime(&now);
}

void PCF2129_RTC::writeTime(const tm& datetime)
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_SECONDS);
    Wire.write(decToBcd(datetime.tm_sec) | _BV(7));
    Wire.write(decToBcd(datetime.tm_min));
    Wire.write(decToBcd(datetime.tm_hour));
    Wire.write(decToBcd(datetime.tm_mday));
    Wire.write(datetime.tm_wday);
    Wire.write(decToBcd(datetime.tm_mon + 1));
    Wire.write(decToBcd(datetime.tm_year - 100));
    Wire.endTransmission();
}

void PCF2129_RTC::writeTime(uint32_t epoch)
{
    struct tm t = *localtime((time_t*)&epoch);
    writeTime(t);
}

void PCF2129_RTC::enableAlarm()
{
    Wire.beginTransmission(address);
    Wire.write(0x01);
    Wire.write(0x02);
    Wire.endTransmission();
}

void PCF2129_RTC::writeAlarm(const tm& datetime)
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_ALARM_SECONDS);
    Wire.write(decToBcd(datetime.tm_sec) & 0x7F);
    Wire.write(decToBcd(datetime.tm_min) & 0x7F);
    Wire.write(decToBcd(datetime.tm_hour) & 0x7F);
    Wire.write(decToBcd(datetime.tm_mday) & 0x7F);
    Wire.endTransmission();
}

void PCF2129_RTC::writeAlarm(uint32_t alarm_epoch)
{
    struct tm t = *localtime((time_t*)&alarm_epoch);
    writeAlarm(t);
}

void PCF2129_RTC::setSysTime()
{
    timeval ctime{static_cast<time_t>(readTimeEpoch()), 0};
    settimeofday(&ctime, nullptr);
}