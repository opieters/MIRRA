#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <FS.h>
#include <HardwareSerial.h>
#include <LittleFS.h>
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
    template <class T> static constexpr std::string_view rawTypeToFormatSpecifier();
    template <class... Ts> static constexpr auto createFormatString();

public:
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
    return timeLength + levelString.size() - 1;
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

// TODO: Clean up the type to format specifier compile-time functionality with proper SFINAE. This here works, but it's messy, requires two functions, and is
// unruly to extend.

template <class T> constexpr std::string_view Log::typeToFormatSpecifier()
{
    using rawType = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_enum_v<rawType>)
    {
        using underlyingType = std::underlying_type_t<rawType>;
        if constexpr (std::is_same_v<std::make_unsigned_t<underlyingType>, unsigned char>)
        {
            if constexpr (std::is_signed_v<underlyingType>)
            {
                return rawTypeToFormatSpecifier<signed int>();
            }
            else
            {
                return rawTypeToFormatSpecifier<unsigned int>();
            }
        }
        else
        {
            return rawTypeToFormatSpecifier<underlyingType>();
        }
    }
    else
    {
        return rawTypeToFormatSpecifier<rawType>();
    }
}

template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<const char*>() { return "%s"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<char*>() { return "%s"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<signed int>() { return "%i"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<unsigned int>() { return "%u"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<signed char>() { return "%i"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<unsigned char>() { return "%u"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<float>() { return "%f"; };
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<char>() { return "%c"; };

template <class... Ts> constexpr auto Log::createFormatString()
{
    std::array<char, (typeToFormatSpecifier<Ts>().size() + ... + 1)> fmt{0};
    size_t i{0};
    for (const auto& s : std::array{typeToFormatSpecifier<Ts>()...})
    {
        for (auto c : s)
            fmt[i++] = c;
    }
    return fmt;
}

template <Log::Level level, class... Ts> void Log::printv(Ts... args)
{
    if (level < this->level)
        return;
    time_t ctime{time(nullptr)};
    tm* time{gmtime(&ctime)};
    size_t cur{printPreamble<level>(*time)};
    size_t left{sizeof(buffer) - cur};
    constexpr auto fmt{createFormatString<decltype(args)...>()};
    snprintf(&buffer[cur], left, fmt.data(), args...);
    logfilePrint(*time);
    logSerial->println(buffer);
}

#endif