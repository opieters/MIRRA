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

/// @brief This class implements the communication with the PCF2129 through i2c.
class PCF2129_RTC
{
private:
    /// @brief The pin to which the RTC's interrupt pin is connected.
    uint8_t intPin;
    /// @brief The RTC's I2C address.
    uint8_t address;

    /// @brief Helper function that converts two nibbles representing a BCD number to a byte representing a decimal one
    uint8_t bcdToDec(uint8_t value);
    /// @brief Helper function that converts a byte representing a decimal number to two nibbles representing a BCD one.
    uint8_t decToBcd(uint8_t value);

public:
    PCF2129_RTC(uint8_t intPin, uint8_t address);

    struct tm readTime();
    /// @return Returns the UNIX epoch in seconds as read from the RTC.
    uint32_t readTimeEpoch();

    void enableAlarm();
    /// @brief Writes an alarm to the RTC with the given time. The alarm can't be further away than 1 month from now.
    /// @param alarmEpoch The time struct time at which the alarm has to be triggered.
    void writeAlarm(const tm& alarmDatetime);
    /// @brief Writes an alarm to the RTC with the given time. The alarm can't be further away than 1 month from now.
    /// @param alarmEpoch The UNIX epoch in seconds at which the alarm has to be triggered.
    void writeAlarm(uint32_t alarmEpoch);

    /// @brief Writes the given time to the RTC
    /// @param datetime The time struct time to which the RTC should be configured.
    void writeTime(const tm& datetime);
    /// @brief Writes the given time to the RTC
    /// @param epoch The UNIX epoch in seconds to which the RTC should be configured.
    void writeTime(uint32_t epoch);

    /// @brief Satures the system time with the time read from the RTC.
    void setSysTime();
    /// @return The system time.
    uint32_t getSysTime() { return static_cast<uint32_t>(time(nullptr)); }

    /// @return The pin to which the RTC's interrupt pin is connected.
    uint8_t getIntPin() { return intPin; };
};
#endif