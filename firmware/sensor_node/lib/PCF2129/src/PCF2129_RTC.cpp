#include "PCF2129_RTC.h"
#include "Arduino.h"

PCF2129_RTC::PCF2129_RTC(uint8_t int_pin, uint8_t i2c_address)
{
    this->address = i2c_address;
    this->int_pin = int_pin;

    pinMode(int_pin, INPUT_PULLUP);
}

bool PCF2129_RTC::begin()
{
    // I2C config

    // enabling interrupt + clearing flag
    Wire.beginTransmission(address);
    Wire.write(0x01);
    Wire.write(0x02);
    return Wire.endTransmission();
}

void PCF2129_RTC::enable_alarm()
{
    // enabling interrupt + clearing flag
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
    while (!Wire.available());
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
    now.tm_mon = months;
    now.tm_year = years;
    return now;
}

void PCF2129_RTC::write_time(struct tm datetime)
{
    Wire.beginTransmission(address);
    Wire.write(PCF2129_SECONDS);
    Wire.write(dec_to_bcd(datetime.tm_sec) + 0x80); 
    Wire.write(dec_to_bcd(datetime.tm_min));
    Wire.write(dec_to_bcd(datetime.tm_hour));
    Wire.write(dec_to_bcd(datetime.tm_mday));
    Wire.write(datetime.tm_wday);
    Wire.write(dec_to_bcd(datetime.tm_mon));
    Wire.write(dec_to_bcd(datetime.tm_year));
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
    return (uint8_t)((value / 16 * 10) + (value % 16));
}

uint8_t PCF2129_RTC::dec_to_bcd(uint8_t value)
{
    return (uint8_t)((value / 10 * 16) + (value % 10));
}


uint32_t PCF2129_RTC::read_time_epoch()
{
    struct tm now = read_time();
    // To function below uses 1900 as reference year
    now.tm_year += 100;
    // The month index starts at 0
    --now.tm_mon;
    return mktime(&now);
}

void PCF2129_RTC::write_time_epoch(uint32_t epoch)
{   
    struct tm t = *localtime((time_t*)&epoch);
    // The year we have to write is relative to 2000 
    t.tm_year -= 100;
    // The month we have to write starts at 1
    ++t.tm_mon;
    write_time(t);
}

void PCF2129_RTC::write_alarm_epoch(uint32_t alarm_epoch)
{
    struct tm t = *localtime((time_t*)&alarm_epoch);
    // The year and month are not used for the alarm
    write_alarm(t);
}