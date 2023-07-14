#include "logging.h"

void Log::generateLogfilePath(char* buffer, const struct tm& time) { strftime(buffer, 12, "/%Y-%m-%d", &time); }

void Log::removeOldLogfiles(struct tm& time)
{
    uint32_t deleteEpoch{mktime(&time) - daysToKeep * 24 * 60 * 60};
    tm deleteTime = *localtime(reinterpret_cast<time_t*>(&deleteEpoch));
    tm fileTime{};
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        const char* fileName = file.name();
        if (strcmp(strrchr(fileName, '.'), ".log") == 0)
        {
            strptime(&strrchr(fileName, '.')[-10], "%Y-%m-%d.log", &fileTime);
            if ((fileTime.tm_year < deleteTime.tm_year) || (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon < deleteTime.tm_mon) ||
                (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon == deleteTime.tm_mon && fileTime.tm_mday <= deleteTime.tm_mday))
            {
                if (!LittleFS.remove(file.path()))
                {
                    this->logfileEnabled = false;
                    this->error("Could not remove logfile. Log will disable logging to file.");
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

void Log::openLogfile(const struct tm& time)
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
        this->error("Unable to open/create logfile! Log will disable logging to file.");
    }
}

void Log::logfilePrint(struct tm& time)
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
    logfile.println(buffer);
}

void Log::close()
{
    this->logfile.close();
    this->logfileEnabled = false;
}

constexpr std::string_view Log::levelToString(Level level)
{
    switch (level)
    {
    case Level::ERROR:
        return "ERROR: ";
    case Level::INFO:
        return "INFO: ";
    case Level::DEBUG:
        return "DEBUG: ";
    }
    return "NONE: ";
}
