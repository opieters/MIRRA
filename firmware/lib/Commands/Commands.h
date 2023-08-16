#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <Arduino.h>
#include <LittleFS.h>
#include <array>
#include <optional>

#define UART_PHASE_TIMEOUT (1 * 60) // s, length of UART inactivity required to automatically exit command phase

/// @brief Class that implements command functionality for a MIRRAModule (or any class) T, and some basic commands.
/// @tparam Class to implement commands for.
template <class T> class BaseCommands
{
protected:
    /// @brief Maximum line length in characters when entering commands.
    static constexpr size_t lineMaxLength{256};

    /// @brief Enumeration describing the different states the execution of a command can feed back to the commands subsystem.
    enum CommandCode : uint8_t
    {
        COMMAND_NOT_FOUND,
        COMMAND_FOUND,
        COMMAND_EXIT
    };
    /// @brief Flag that determines if command phase should be entered when prompted.
    bool commandPhaseFlag{false};

    /// @brief Module for which this commands subsystem is implementing commands.
    T* parent;

    /// @brief Initialises the command loop, exiting when the appropriate command code is returned by processCommands.
    void start();
    /// @brief Reads a line from the UART input stream. Helper function to be used during the command phase.
    /// @return An array representing a command sequence entered by the user. Disengaged if this function times out.
    std::optional<std::array<char, lineMaxLength>> readLine();
    /// @brief Reads the command, calls the correct function and its arguments.
    /// @param command Pointer to char buffer holding the command.
    /// @return A command code desribing the result of the execution of the command.
    CommandCode processCommands(char* command);

    /// @brief Lists all files currently available on the filesystem.
    void listFiles();
    /// @brief Prints the given file to the serial output.
    /// @param filename The name of the file to be printed, including slashes.
    /// @param hex Whether to print the file in hexadecimal format or in ASCII.
    void printFile(const char* filename, bool hex = false);
    /// @brief Removes the file from the filesystem.
    /// @param filename The name of the file to be removed.
    void removeFile(const char* filename);
    /// @brief Creates an empty file.
    /// @param filename The name of the empty file to be created.
    void touchFile(const char* filename);

protected:
    /// @brief Creates the commands subsystem.
    /// @param parent Module for which this commands subsystem is implementing commands.
    /// @param flag Whether to sature the commandPhaseFlag or not.
    BaseCommands(T* parent, bool flag = false) : parent{parent}, commandPhaseFlag{flag} {}
    /// @brief Creates the commands subsystem and saturates the commandPhaseFlag.
    /// @param parent Module for which this commands subsystem is implementing commands.
    /// @param checkPin Pin to check for setting the commandPhaseFlag.
    /// @param invert Whether to invert the value of the read pin.
    BaseCommands(T* parent, uint8_t checkPin, bool invert = false) : parent{parent}, commandPhaseFlag{invert != static_cast<bool>(digitalRead(checkPin))} {}

public:
    /// @brief Checks the module's MIRRAModule::commandPhaseFlag and enters the command phase if it is set.
    void prompt();
    // @brief Forcibly sets the commandPhaseFlag to true.
    void setFlag() { commandPhaseFlag = true; };
};

#include "Commands.tpp"
#endif