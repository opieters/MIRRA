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

    /// @brief Serial to print to.
    HardwareSerial* logSerial{nullptr};
    /// @brief Logging level of the logging module. Messages below this level will not be printed.
    Level level{static_cast<uint8_t>(-1)};

    /// @brief Whether logging to file is enabled.
    bool logfileEnabled{false};
    /// @brief Time struct of the currently loaded logging file.
    tm logfileTime{0};
    /// @brief Currently loaded logging file. Should be opened in append mode.
    File logfile{};
    /// @brief Days to keep a logging file in the filesystem.
    static constexpr size_t daysToKeep{7};

    /// @brief Generates a logfile path from a time struct.
    /// @param buffer Buffer to which to write the logfile path.
    /// @param time Time for which to generate a logfile path.
    void generateLogfilePath(char* buffer, const struct tm& time);
    /// @brief Removes all logfiles that have reached the expiry date by a given date.
    /// @param time Date from which logfile expiration is calculated according to daysToKeep.
    void removeOldLogfiles(struct tm& time);
    /// @brief Opens/creates a logfile with the given date.
    /// @param time The date for which to open/create a logfile.
    void openLogfile(const struct tm& time);
    /// @brief Manages all file-related operations for a given date.
    /// @param time The date from which to manage the filesystem.
    void manageLogfile(struct tm& time);
    /// @brief Prints the current buffer to the currently open logfile.
    void logfilePrint();

    /// @brief Buffer in which the final string is constructed and printed from.
    char buffer[256]{0};
    /// @brief Prints the preamble portion of the log line.
    /// @tparam level The level displayed in the preamble.
    /// @param time The time displayed in the preamble.
    /// @return The length of the preamble that was printed.
    template <Level level> size_t printPreamble(const tm& time);
    /// @return String conversion from a level.
    static constexpr std::string_view levelToString(Level level);
    /// @return A format specifier string matched to the type argument.
    template <class T> static constexpr std::string_view typeToFormatSpecifier();
    template <class T> static constexpr std::string_view rawTypeToFormatSpecifier();
    /// @return A format string matched to the given type arguments.
    template <class... Ts> static constexpr auto createFormatString();
    /// @brief Type-safe variadic print function. Uses compile-time format string instantiation to ensure safety in using printf.
    /// @param buffer Buffer to which to print.
    /// @param max Max number of characters to print to buffer.
    template <class... Ts> void printv(char* buffer, size_t max, Ts... args);
    /// @brief Prints to the logging buffer and forwards to (if enabled) the output serial and logfile.
    /// @tparam level Log level of printed message.
    template <Level level, class... Ts> void print(Ts... args);

public:
    /// @param logSerial HardwareSerial object to print to.
    void setSerial(HardwareSerial* logSerial) { this->logSerial = logSerial; }
    /// @param level Level to set the logging module to. Messages below this level will not be printed.
    void setLogLevel(Log::Level level) { this->level = level; }
    /// @brief Enables or disables the logging to file functionality.
    void setLogfile(bool enable = true) { this->logfileEnabled = enable; }

    /// @brief Singleton global log object
    static Log log;

    template <class... Ts> static void debug(Ts... args) { log.print<DEBUG>(args...); }
    template <class... Ts> static void info(Ts... args) { log.print<INFO>(args...); }
    template <class... Ts> static void error(Ts... args) { log.print<ERROR>(args...); }

    /// @brief Closes the logging module, releasing the logging file.
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
template <> constexpr std::string_view Log::rawTypeToFormatSpecifier<long signed int>() { return "%i"; };
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

template <class... Ts> void Log::printv(char* buffer, size_t max, Ts... args)
{
    constexpr auto fmt{createFormatString<Ts...>()};
    snprintf(buffer, max, fmt.data(), args...);
}
template <Log::Level level, class... Ts> void Log::print(Ts... args)
{
    if (level < this->level)
        return;
    time_t ctime{time(nullptr)};
    tm* time{gmtime(&ctime)};
    manageLogfile(*time);
    size_t cur{printPreamble<level>(*time)};
    size_t left{sizeof(buffer) - cur};
    printv(&buffer[cur], left, args...);
    logfilePrint();
    logSerial->println(buffer);
}

#endif