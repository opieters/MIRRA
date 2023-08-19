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

template <class T> class BaseCommands
{
    T* module;
    /// @brief Lists all files currently available on the filesystem.
    CommandCode listFiles();
    /// @brief Prints the given file to the serial output.
    /// @param filename The name of the file to be printed, including slashes.
    /// @param hex Whether to print the file in hexadecimal format or in ASCII.
    CommandCode printFile(const char* filename, bool hex = false);
    /// @brief Removes the file from the filesystem.
    /// @param filename The name of the file to be removed.
    CommandCode removeFile(const char* filename);
    /// @brief Creates an empty file.
    /// @param filename The name of the empty file to be created.
    CommandCode touchFile(const char* filename);

    static constexpr auto baseCommands{
        std::make_tuple(std::make_pair(std::array{"ls", "list"}, &BaseCommands<T>::listFiles), std::make_pair(std::array{"print"}, &BaseCommands<T>::printFile),
                        std::make_pair(std::array{"rm"}, &BaseCommands<T>::removeFile), std::make_pair(std::array{"touch"}, &BaseCommands<T>::touchFile))};

protected:
    BaseCommands(T* module) : module{module} {}
    const T& getModule() { return *module; }
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
    template <class C> typename std::enable_if_t<std::is_base_of_v<BaseCommands, C>, void> prompt(C commands);
    // @brief Forcibly sets the commandPhaseFlag to true.
    void setFlag() { commandPhaseFlag = true; };
};

class CommandParser
{
    /// @brief Maximum line length in characters when entering commands.
    static constexpr size_t lineMaxLength{256};
    /// @brief Reads a line from the UART input stream. Helper function to be used during the command phase.
    /// @return An array representing a command sequence entered by the user. Disengaged if this function times out.
    std::optional<std::array<char, lineMaxLength>> readLine();
    /// @brief Reads the command, calls the correct function and its arguments.
    /// @tparam C Commands set to be used for this command phase.
    /// @param command Pointer to char buffer holding the command.
    /// @return A command code desribing the result of the execution of the command.
    template <class C> CommandCode processCommands(char* command);

public:
    /// @brief Initialises the command loop, exiting when the appropriate command code is returned by processCommands.
    /// @tparam C Commands set to be used for this command phase.
    template <class C> typename std::enable_if_t<std::is_base_of_v<BaseCommands, C>, void> start();
};

#include "Commands.tpp"
#endif