#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <SPIFFS.h>
#include <FS.h>
#include <HardwareSerial.h>
#include <PCF2129_RTC.h>

enum log_level
{
    none,
    error,
    info,
    debug
};

class Logger
{
private:
    log_level level;
    char *logfile_path;
    PCF2129_RTC *rtc;

    File open_logfile();
    void logfile_print(const char *string);
    void print(const char *string);

public:
    Logger(log_level level, char *logfile_path, PCF2129_RTC *rtc);
    void debug(const char *string);
    void info(const char *string);
    void error(const char *string);
};
#endif