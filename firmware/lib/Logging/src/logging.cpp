#include "logging.h"

Log Log::log{};

void Log::generateLogfilePath(char* buffer, const struct tm& time) { strftime(buffer, 16, "/%Y-%m-%d.log", &time); }

void Log::removeOldLogfiles(struct tm& time)
{
    time_t deleteEpoch{mktime(&time) - static_cast<time_t>(daysToKeep * 24 * 60 * 60)};
    tm deleteTime;
    gmtime_r(&deleteEpoch, &deleteTime);
    tm fileTime{0};
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        const char* fileName{file.name()};
        file.close();
        const char* extension;
        if (extension = strstr(fileName, ".log"))
        {
            strptime(&extension[-10], "%Y-%m-%d.log", &fileTime);
            if ((fileTime.tm_year < deleteTime.tm_year) || (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon < deleteTime.tm_mon) ||
                (fileTime.tm_year == deleteTime.tm_year && fileTime.tm_mon == deleteTime.tm_mon && fileTime.tm_mday < deleteTime.tm_mday))
            {
                char buffer[32]{0};
                snprintf(buffer, sizeof(buffer), "/%s", fileName);
                if (!LittleFS.remove(buffer))
                    this->error("Could not remove logfile '", buffer, "'.");
                else
                    this->info("Removed old logfile '", buffer, "'.");
            }
        }
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
    if ((this->logfileTime.tm_mday != time.tm_mday) || (this->logfileTime.tm_mon != time.tm_mon) || (this->logfileTime.tm_year != time.tm_year) ||
        (!this->logfile))
    {
        if (this->logfile)
            this->logfile.close();
        openLogfile(time);
        if (!this->logfile)
            return;
        removeOldLogfiles(time);
    }
    logfile.println(buffer);
}

void Log::close()
{
    this->logfile.close();
    this->logfileEnabled = false;
}
