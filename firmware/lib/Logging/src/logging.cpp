#include "logging.h"

Logger::Logger(log_level level, char *logfile_path = "/logs.dat", PCF2129_RTC *rtc)
{
    this->level = level;
    this->logfile_path = logfile_path;
    this->rtc = rtc;
    open_logfile(); // preliminary test to see if the logfile is valid
}
File Logger::open_logfile()
{
    File logfile;
    if (SPIFFS.exists(this->logfile_path))
    {
        logfile = SPIFFS.open(this->logfile_path, FILE_APPEND);
    }
    else
    {
        logfile = SPIFFS.open(this->logfile_path, FILE_WRITE);
    }

    if (!logfile)
    {
        this->logfile_path = nullptr;
        error("Unable to open/create logfile! Logger will disable logging to file.");
    }
    return logfile;
}

void Logger::logfile_print(const char *string)
{
    if (this->logfile_path == nullptr)
        return;
    File logfile = open_logfile();
    for (size_t i = 0; i != '\0'; i++)
    {
        logfile.write(string[i]);
    }
    logfile.close();
}

void Logger::print(const char *string)
{
    char string_buffer[128];
    if (this->rtc != nullptr)
    {
        struct tm time = this->rtc->read_time();
        snprintf(string_buffer, 128, "[%04u-%02u-%02u-%02u %02u:%02u:%02u] %s", time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, string);
    }
    else
    {
        snprintf(string_buffer, 128, "%s", string);
    }
    logfile_print(string_buffer);
    Serial.print(string_buffer);
}

void Logger::error(const char *string)
{
    if (this->level < log_level::error)
        return;
    this->print(string);
}

void Logger::info(const char *string)
{
    if (this->level < log_level::info)
        return;
    this->print(string);
}

void Logger::debug(const char *string)
{
    if (this->level < log_level::debug)
        return;
    this->print(string);
}
