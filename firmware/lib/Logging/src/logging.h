#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <FS.h>
#include <HardwareSerial.h>
#include <LittleFS.h>
#include <PCF2129_RTC.h>
#include <type_traits>

class Log
{
public:
    enum Level : uint8_t
    {
        DEBUG,
        INFO,
        ERROR
    };

private:
    Log() = default;

    PCF2129_RTC* rtc{nullptr};
    HardwareSerial* logSerial{nullptr};
    Level level{static_cast<uint8_t>(-1)};

    bool logfileEnabled{false};
    tm logfileTime{0};
    File logfile{};
    static const size_t daysToKeep{7};
    void generateLogfilePath(char* buffer, const struct tm& time);
    void removeOldLogfiles(struct tm& time);
    void openLogfile(const struct tm& time);
    void logfilePrint(struct tm& time);

    char buffer[256]{0};
    template <Level level> size_t printPreamble(const tm& time);
    static constexpr std::string_view levelToString(Level level);
    template <class T> static constexpr std::string_view typeToFormatSpecifier();

public:
    void setRTC(PCF2129_RTC* rtc) { this->rtc = rtc; }
    void setSerial(HardwareSerial* logSerial) { this->logSerial = logSerial; }
    void setLogLevel(Log::Level level) { this->level = level; }
    void setLogfile(bool enable) { this->logfileEnabled = enable; }

    static Log log;

    template <Level level, class... Ts> void printv(Ts... args);
    template <class... Ts> static void debug(Ts... args) { log.printv<DEBUG>(args...); }
    template <class... Ts> static void info(Ts... args) { log.printv<INFO>(args...); }
    template <class... Ts> static void error(Ts... args) { log.printv<ERROR>(args...); }

    void close();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log() = default;
};

template <Log::Level level> size_t Log::printPreamble(const tm& time)
{
    constexpr size_t timeLength{sizeof("[0000-00-00 00:00:00]")};
    strftime(buffer, timeLength, "[%F %T]", &time);
    constexpr std::string_view levelString{levelToString(level)};
    strcpy(&buffer[timeLength - 1], levelString.cbegin());
    return timeLength + levelString.size() + 1;
}

template <> constexpr std::string_view Log::typeToFormatSpecifier<char*>() { return "%s"; };
template <> constexpr std::string_view Log::typeToFormatSpecifier<const char*>() { return "%s"; };
template <> constexpr std::string_view Log::typeToFormatSpecifier<signed int>() { return "%i"; };
template <> constexpr std::string_view Log::typeToFormatSpecifier<unsigned int>() { return "%u"; };
template <> constexpr std::string_view Log::typeToFormatSpecifier<float>() { return "%f"; };

template <Log::Level level, class... Ts> void Log::printv(Ts... args)
{
    if (level < this->level)
        return;
    time_t ctime{time(nullptr)};
    tm* time{gmtime(&ctime)};
    size_t cur = printPreamble<level>(*time);
    size_t left = sizeof(buffer) - cur;
    std::array<char, (typeToFormatSpecifier<typename std::remove_cv<typename std::remove_reference<decltype(args)>::type>::type>().size() + ... + 1)> fmt{0};
    auto join = [&fmt, i = 0](const auto& s) mutable
    {
        for (auto c : typeToFormatSpecifier<typename std::remove_cv<typename std::remove_reference<decltype(s)>::type>::type>())
            fmt[i++] = c;
    };
    (join(args), ...);
    snprintf(&buffer[left], left, fmt.cbegin(), args...);
    logfilePrint(*time);
    logSerial->println(buffer);
}

#endif