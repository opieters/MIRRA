#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <Arduino.h>
#include <LittleFS.h>

#include <array>
#include <optional>

#define UART_PHASE_TIMEOUT (1 * 60) // s, length of UART inactivity required to automatically exit command phase

enum CommandCode : uint8_t
{
    COMMAND_NOT_FOUND,
    COMMAND_ERROR,
    COMMAND_SUCCESS,
    COMMAND_EXIT
};

template <size_t nAliases, class C, class... CommandArgs>
struct CommandAliasesPair : private std::pair<CommandCode (C::*)(CommandArgs...), std::array<const char*, nAliases>>
{
    typedef std::array<const char*, nAliases> AliasesArray;
    typedef CommandCode (C::*Command)(CommandArgs...);
    typedef std::pair<Command, AliasesArray> Pair;

    template <class... Aliases>
    constexpr CommandAliasesPair(Command&& f, Aliases&&... aliases) : Pair(std::forward<Command>(f), {{std::forward<Aliases>(aliases)...}})
    {
    }

    constexpr Command getCommand() const { return std::get<0>(*this); }
    constexpr AliasesArray getAliases() const { return std::get<1>(*this); }
    constexpr size_t getCommandArgCount() const { return sizeof...(CommandArgs); }
};

template <class... Aliases, class C, class... CommandArgs>
CommandAliasesPair(CommandCode (C::*&& f)(CommandArgs...), Aliases&&... aliases) -> CommandAliasesPair<sizeof...(Aliases), C, CommandArgs...>;

struct CommonCommands
{
    /// @brief Lists all files currently available on the filesystem.
    CommandCode listFiles();
    /// @brief Prints the given file to the serial output.
    /// @param filename The name of the file to be printed, including slashes.
    CommandCode printFile(const char* filename);
    /// @brief Prints the given file to the serial output, interpreted in hexadecimal notation.
    /// @param filename The name of the file to be printed, including slashes.
    CommandCode printFileHex(const char* filename);
    /// @brief Removes the file from the filesystem.
    /// @param filename The name of the file to be removed.
    CommandCode removeFile(const char* filename);
    /// @brief Creates an empty file.
    /// @param filename The name of the empty file to be created.
    CommandCode touchFile(const char* filename);
    /// @brief Formats the filesystem. This erases all data!
    CommandCode format();
    /// @brief Echoes the argument to the UART output. Used for testing CLI functionality.
    /// @param arg The argument to echo.
    CommandCode echo(const char* arg);
    /// @brief Exits command mode.
    CommandCode exit();

    static constexpr auto getCommands()
    {
        return std::make_tuple(
            CommandAliasesPair(&CommonCommands::listFiles, "ls", "list"), CommandAliasesPair(&CommonCommands::printFile, "print", "printfile"),
            CommandAliasesPair(&CommonCommands::printFileHex, "printhex", "printfilehex"), CommandAliasesPair(&CommonCommands::removeFile, "rm", "remove"),
            CommandAliasesPair(&CommonCommands::touchFile, "touch"), CommandAliasesPair(&CommonCommands::format, "format"),
            CommandAliasesPair(&CommonCommands::echo, "echo"), CommandAliasesPair(&CommonCommands::exit, "exit", "close"));
    };
};

/// @brief Class responsible for entry into the commands system loop set by CommandParser.
class CommandEntry
{
    /// @brief Flag that determines if command phase should be entered when prompted.
    bool commandPhaseFlag{false};

public:
    /// @brief Creates the commands entry subsystem.
    /// @param flag Whether to sature the commandPhaseFlag or not.
    CommandEntry(bool flag = false) : commandPhaseFlag{flag} {}
    /// @brief Creates the commands entry subsystem and saturates its commandPhaseFlag.
    /// @param checkPin Pin to check for setting the commandPhaseFlag.
    /// @param invert Whether to invert the value of the read pin.
    CommandEntry(uint8_t checkPin, bool invert = false) : commandPhaseFlag{invert != static_cast<bool>(digitalRead(checkPin))} {}
    /// @brief Checks the commandPhaseFlag and enters the command phase if it is set.
    /// @tparam C Commands set to be used for this command phase.
    template <class C> typename std::enable_if_t<std::is_base_of_v<CommonCommands, C>, void> prompt(C&& commands);
    // @brief Forcibly sets the commandPhaseFlag to true.
    void setFlag() { commandPhaseFlag = true; };
};

class CommandParser
{
    /// @brief Maximum line length in characters when entering commands.
    static constexpr size_t lineMaxLength{256};
    /// @brief Reads the command, calls the correct function with appropriate arguments.
    /// @tparam C Commands set to be used for this command phase.
    /// @param line Pointer to char buffer holding the command.
    /// @return A command code desribing the result of the execution of the command.
    template <class C, size_t I = 0> CommandCode parseLine(char* line, C&& commands);

public:
    /// @brief Reads a line from the UART input stream. Helper function to be used during the command phase.
    /// @return An array representing a command sequence entered by the user. Disengaged if this function times out.
    static std::optional<std::array<char, lineMaxLength>> readLine();
    /// @brief Initialises the command loop, exiting when the appropriate command code is returned by processCommands.
    /// @tparam C Commands set to be used for this command phase.
    template <class C> typename std::enable_if_t<std::is_base_of_v<CommonCommands, C>, void> start(C&& commands);
};

#include "Commands.tpp"
#endif