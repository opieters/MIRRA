#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <FS.h>
#include <HardwareSerial.h>
#include <LittleFS.h>
#include <PCF2129_RTC.h>

class Logger
{
public:
    enum Level
    {
        debug,
        info,
        error
    };

private:
    PCF2129_RTC* rtc;

    Level level;

    char logfileBasePath[22]{0};
    bool logfileEnabled{true};
    struct tm logfileTime;
    File logfile;

    void generateLogfilePath(char* buffer, const struct tm& time);
    void removeOldLogfiles(struct tm& time);
    void openLogfile(const struct tm& time);
    void logfilePrint(const char* string, struct tm& time);

    const char* levelToString(Level level) const;

public:
    Logger(Level level, const char* logfileBasePath, PCF2129_RTC* rtc);
    void printf(Level level, const char* fmt, ...);
    void print(Level level, const char* string);
    void print(Level level, const signed int i);
    void print(Level level, const unsigned int i);
    void close();

    static const size_t daysToKeep = 7;
};
#endif