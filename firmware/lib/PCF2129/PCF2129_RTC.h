#ifndef PCF2129_RTC_H
#define PCF2129_RTC_H

#include <Wire.h>
#include <time.h>

#define PCF2129_CONTROL_1_REGISTER 0x00
#define PCF2129_CONTROL_2_REGISTER 0x01
#define PCF2129_CONTROL_3_REGISTER 0x02
#define PCF2129_CONTROL_12_24 0x04
#define PCF2129_SECONDS 0x03
#define PCF2129_MINUTES 0x04
#define PCF2129_HOURS 0x05
#define PCF2129_DAYS 0x06
#define PCF2129_WEEKDAYS 0x07
#define PCF2129_MONTHS 0x08
#define PCF2129_YEARS 0x09

#define PCF2129_ALARM_SECONDS 0x0A
#define PCF2129_ALARM_MINUTES 0x0B
#define PCF2129_ALARM_HOURS 0x0C
#define PCF2129_ALARM_DAYS 0x0D
#define PCF2129_ALARM_WEEKDAYS 0x0E

/**
 * This class implements the communication with the PCF2129 through i2c.
 **/
class PCF2129_RTC
{
private:
    uint8_t int_pin;
    uint8_t address;
    /**
     * This buffer is used to convert the written or read time to an epoch
     */
    uint8_t bcd_to_dec(uint8_t value);
    uint8_t dec_to_bcd(uint8_t value);

public:
    PCF2129_RTC(uint8_t int_pin, uint8_t address);
    void enable_alarm();
    struct tm read_time();
    void write_time(struct tm datetime);
    void write_alarm(struct tm alarm_datetime);

    /**
     * Reads the time from the rtc and returns an epoch
     */
    uint32_t read_time_epoch();

    /**
     * @brief writes the time to the RTC
     *
     * @param epoch The epoch time (seconds sinds 1/1/1970)
     */
    void write_time_epoch(uint32_t epoch);

    /**
     * @brief Writes an alarm to the given epoch time the alarm can't be further away than 1 month from now
     *
     * @param alarm_epoch The epoch time at which the alarm has to be triggered
     */
    void write_alarm_epoch(uint32_t alarm_epoch);

    uint8_t getIntPin() { return int_pin; };
};
#endif