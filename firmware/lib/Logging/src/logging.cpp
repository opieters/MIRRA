#include "logging.h"

Logger::Logger(Level level, const char* logfileBasePath, PCF2129_RTC* rtc)
    : rtc{rtc}, level{level}, logfileEnabled{logfileBasePath != nullptr}, logfileTime{rtc->read_time()}
{
    if (logfileBasePath != nullptr)
        strncpy(this->logfileBasePath, logfileBasePath, 18);
    this->print(Logger::debug, "Logger initialised.");
}

void Logger::generateLogfilePath(char* buffer, const struct tm& time)
{
    char timeString[11];
    strftime(timeString, 11, "%Y-%m-%d", &time);
    snprintf(buffer, 32, "%s%s.log", this->logfileBasePath, timeString);
}

void Logger::removeOldLogfiles(struct tm& time)
{
    uint32_t deleteEpoch{mktime(&time) - daysToKeep * 24 * 60 * 60};
    struct tm deleteTime = *localtime(reinterpret_cast<time_t*>(&deleteEpoch));
    struct tm fileTime
    {
    };
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        const char* fileName = file.name();
        if (strcmp(strrchr(fileName, '.'), ".log") == 0)
        {
            strptime(fileName, "%Y-%m-%d.log", &fileTime);
            if ((fileTime.tm_year < deleteTime.tm_year) || (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon < deleteTime.tm_mon) ||
                (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon == deleteTime.tm_mon && fileTime.tm_mday <= deleteTime.tm_mday))
            {
                if (!LittleFS.remove(file.path()))
                {
                    this->logfileEnabled = false;
                    this->print(Logger::error, "Could not remove logfile. Logger will disable logging to file.");
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
}

void Logger::openLogfile(const struct tm& time)
{
    char logfilePath[32];
    generateLogfilePath(logfilePath, time);
    if (LittleFS.exists(logfilePath))
    {
        this->logfile = LittleFS.open(logfilePath, "a");
    }
    else
    {
        this->logfile = LittleFS.open(logfilePath, "w", true);
    }
    this->logfileTime = time;
    if (!this->logfile)
    {
        this->logfileEnabled = false;
        this->print(Level::error, "Unable to open/create logfile! Logger will disable logging to file.");
    }
}

void Logger::logfilePrint(const char* string, struct tm& time)
{
    if (!this->logfileEnabled)
        return;
    if ((!this->logfile) || (this->logfileTime.tm_mday != time.tm_mday) || (this->logfileTime.tm_mon != time.tm_mon) ||
        (this->logfileTime.tm_year != time.tm_year))
    {
        removeOldLogfiles(time);
        if (this->logfile)
            this->logfile.close();
        openLogfile(time);
        if (!this->logfile)
            return;
    }
    logfile.println(string);
}

void Logger::printf(Level level, const char* fmt, ...)
{
    if (level < this->level)
        return;
    char buffer[256];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    this->print(level, buffer);
}

void Logger::print(Level level, const char* string)
{
    if (level < this->level)
        return;
    size_t time_length = 21;
    size_t level_length = 5;
    size_t len_buffer = time_length + 1 + level_length + 2 + strlen(string) + 1;
    char string_buffer[len_buffer];
    char time_string[time_length + 1];
    const char* levelString{levelToString(level)};

    struct tm time = this->rtc->read_time();

    strftime(time_string, sizeof(time_string), "[%F %T]", &time);
    snprintf(string_buffer, len_buffer, "%s %s: %s", time_string, levelString, string);

    Serial.println(string_buffer);
    logfilePrint(string_buffer, time);
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

void Logger::close()
{
    this->logfile.close();
    this->logfileEnabled = false;
}

const char* Logger::levelToString(Level level) const
{
    switch (level)
    {
    case Level::error:
        return "ERROR";
    case Level::info:
        return "INFO";
    case Level::debug:
        return "DEBUG";
    }
    return "NONE";
}
