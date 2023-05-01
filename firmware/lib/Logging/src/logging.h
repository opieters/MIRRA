#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <SPIFFS.h>
#include <FS.h>
#include <HardwareSerial.h>
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
    Level level;
    char logfile_base_path[22];
    PCF2129_RTC *rtc;

    bool logfile_enabled = true;
    struct tm logfile_time;
    File logfile;
    void generate_logfile_path(char *buffer, struct tm &time);
    void delete_oldest_logfile(struct tm &time);
    File open_logfile(char *logfile_path);
    void logfile_print(const char *string, struct tm &time);

    char *level_to_string(Level level, char *buffer, size_t buffer_length);

public:
    Logger(Level level, const char *logfile_path, PCF2129_RTC *rtc);
    void printf(Level level, const char *fmt, ...);
    void print(Level level, const char *string);
    void print(Level level, const signed int i);
    void print(Level level, const unsigned int i);

    static const size_t days_to_keep = 7;
};
#endif