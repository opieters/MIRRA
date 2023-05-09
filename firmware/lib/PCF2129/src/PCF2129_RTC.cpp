#include "PCF2129_RTC.h"
#include "Arduino.h"

PCF2129_RTC::PCF2129_RTC(uint8_t int_pin, uint8_t address) : address{address}, int_pin{int_pin}
{
    pinMode(int_pin, INPUT_PULLUP);
}

void PCF2129_RTC::enable_alarm()
{
    Wire.beginTransmission(address);
    Wire.write(0x01);
    Wire.write(0x02);
    Wire.endTransmission();
}

struct tm PCF2129_RTC::read_time()
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_SECONDS);
    Wire.endTransmission();
    Wire.requestFrom(address, (uint8_t)7);
    while (!Wire.available())
        ;
    uint8_t seconds = bcd_to_dec(Wire.read());
    uint8_t minutes = bcd_to_dec(Wire.read());
    uint8_t hours = bcd_to_dec(Wire.read());
    uint8_t days = bcd_to_dec(Wire.read());
    uint8_t week_day = bcd_to_dec(Wire.read());
    uint8_t months = bcd_to_dec(Wire.read());
    uint16_t years = bcd_to_dec(Wire.read());

    struct tm now;
    now.tm_sec = seconds;
    now.tm_min = minutes;
    now.tm_hour = hours;
    now.tm_mday = days;
    now.tm_wday = week_day;
    now.tm_mon = months - 1;   // (RTC uses 1-12 as months but C time structs use 0-11 as months)
    now.tm_year = years + 100; // (RTC uses 2000 as reference year but C time structs use 1900 as reference year)
    return now;
}

void PCF2129_RTC::write_time(struct tm datetime)
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_SECONDS);
    Wire.write(dec_to_bcd(datetime.tm_sec) | _BV(7));
    Wire.write(dec_to_bcd(datetime.tm_min));
    Wire.write(dec_to_bcd(datetime.tm_hour));
    Wire.write(dec_to_bcd(datetime.tm_mday));
    Wire.write(datetime.tm_wday);
    Wire.write(dec_to_bcd(datetime.tm_mon + 1));
    Wire.write(dec_to_bcd(datetime.tm_year - 100));
    Wire.endTransmission();
}

void PCF2129_RTC::write_alarm(struct tm datetime)
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_ALARM_SECONDS);
    Wire.write(dec_to_bcd(datetime.tm_sec) & 0x7F);
    Wire.write(dec_to_bcd(datetime.tm_min) & 0x7F);
    Wire.write(dec_to_bcd(datetime.tm_hour) & 0x7F);
    Wire.write(dec_to_bcd(datetime.tm_mday) & 0x7F);
    Wire.endTransmission();
}

uint8_t PCF2129_RTC::bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

uint8_t PCF2129_RTC::dec_to_bcd(uint8_t value)
{
    return (uint8_t)((value / 10 * 16) + (value % 10));
}

uint32_t PCF2129_RTC::read_time_epoch()
{
    struct tm now = read_time();
    return mktime(&now);
}

void PCF2129_RTC::write_time_epoch(uint32_t epoch)
{
    struct tm t = *localtime((time_t *)&epoch);
    write_time(t);
}

void PCF2129_RTC::write_alarm_epoch(uint32_t alarm_epoch)
{
    struct tm t = *localtime((time_t *)&alarm_epoch);
    write_alarm(t);
}