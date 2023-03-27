#include "logging.h"

Logger::Logger(Level level, char *logfile_base_path, PCF2129_RTC *rtc)
{
    this->level = level;
    if (logfile_base_path == nullptr)
    {
        this->logfile_enabled = false;
    }
    else
    {
        strncpy(this->logfile_base_path, logfile_base_path, 22);
    }
    this->rtc = rtc;
    this->logfile_time = rtc->read_time();
}
File Logger::open_logfile(char *logfile_path)
{
    File logfile;
    if (SPIFFS.exists(logfile_path))
    {
        logfile = SPIFFS.open(logfile_path, FILE_APPEND);
    }
    else
    {
        logfile = SPIFFS.open(logfile_path, FILE_WRITE);
    }

    if (!logfile)
    {
        this->logfile_enabled = false;
        this->print(Level::error, "Unable to open/create logfile! Logger will disable logging to file.");
    }
    return logfile;
}

void Logger::logfile_print(const char *string, struct tm &time)
{
    if (!this->logfile_enabled)
        return;
    if ((!this->logfile) || (this->logfile_time.tm_mday != time.tm_mday) || (this->logfile_time.tm_mon != time.tm_mon) || (this->logfile_time.tm_year != time.tm_year))
    {
        if (this->logfile)
            logfile.close();
        char time_string[11];
        strftime(time_string, 11, "%F", &time);
        char logfile_path[32];
        snprintf(logfile_path, 32, "%s%s", logfile_base_path, time_string);
        this->logfile = open_logfile(logfile_path);
        if (!this->logfile)
            return;
    }
    logfile.println(string);
}

void Logger::print(Level level, const char *string)
{
    if (level < this->level)
        return;
    size_t time_length = 21;
    size_t level_length = 5;
    size_t len_buffer = time_length + 1 + level_length + 2 + strlen(string) + 1;
    char string_buffer[len_buffer];
    char time_string[time_length + 1];
    char level_string[level_length + 1];

    struct tm time = this->rtc->read_time();

    strftime(time_string, sizeof(time_string), "[%F %T]", &time);
    level_to_string(level, level_string, sizeof(level_string));
    snprintf(string_buffer, len_buffer, "%s %s: %s", time_string, level_string, string);

    logfile_print(string_buffer, time);
    Serial.print(string_buffer);
}

void Logger::print(Level level, const unsigned int u)
{
    char int_string[21];
    snprintf(int_string, 21, "%u", u);
    print(level, int_string);
}

void Logger::print(Level level, const signed int i)
{
    char int_string[22];
    snprintf(int_string, 22, "%i", i);
    print(level, int_string);
}

char *Logger::level_to_string(Level level, char *buffer, size_t buffer_length)
{
    switch (level)
    {
    case Level::error:
        strncpy(buffer, "ERROR", buffer_length);
        return buffer;
    case Level::info:
        strncpy(buffer, "INFO", buffer_length);
        return buffer;
    case Level::debug:
        strncpy(buffer, "DEBUG", buffer_length);
        return buffer;
    }
}
