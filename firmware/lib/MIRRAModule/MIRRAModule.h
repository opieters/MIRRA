#ifndef __MIRRAMODULE_H__
#define __MIRRAMODULE_H__

#include "LoRaModule.h"
#include "PCF2129_RTC.h"
#include <Arduino.h>
#include <CommunicationCommon.h>
#include <FS.h>
#include <LittleFS.h>
#include <logging.h>

#define LOG_FP "/"
#define LOG_LEVEL Log::DEBUG

#define UART_PHASE_TIMEOUT (1 * 60) // s, length of UART inactivity required to automatically exit command phase

/// @brief A base class for MIRRA modules to inherit from, which implements common functionality.
class MIRRAModule
{
public:
    /// @brief Struct that groups pin configurations for the initialisation of a MIRRAModule.
    struct MIRRAPins
    {
        uint8_t bootPin;
        uint8_t peripheralPowerPin;
        uint8_t sdaPin;
        uint8_t sclPin;
        uint8_t csPin;
        uint8_t rstPin;
        uint8_t dio0Pin;
        uint8_t txPin;
        uint8_t rxPin;
        uint8_t rtcIntPin;
        uint8_t rtcAddress;
    };
    /// @brief Configures the ESP32's pins, starts basic communication primitives (UART, I2C) and mounts the filesystem. Must be called before initialisation of
    /// the MIRRAModule.
    /// @param pins The pin configuration for the MIRRAModule.
    static void prepare(const MIRRAPins& pins);

protected:
    /// @brief Initialises the MIRRAModule, RTC, LoRa and logging modules.
    /// @param pins The pin configuration for the MIRRAModule.
    MIRRAModule(const MIRRAPins& pins);
    /// @brief Stores the given sensor data messages into the module's flash filesystem.
    /// @param m The message to be stored.
    /// @param dataFile File object opened in append mode.
    void storeSensorData(const Message<SENSOR_DATA>& m, File& dataFile);
    /// @brief Prunes the sensor data file if the filesize exceeds the maxSize argument, otherwise does nothing.
    /// @param dataFile The file object to be pruned, opened in a read mode at the start of the file.
    /// @param maxSize The max file size in bytes.
    void pruneSensorData(File&& dataFile, uint32_t maxSize);

    /// @brief Enters deep sleep for the specified time.
    /// @param sleepTime The time in seconds to sleep.
    void deepSleep(uint32_t sleepTime);
    /// @brief Enters deep sleep until the specified time.
    /// @param untilTime The time (UNIX epoch, seconds) the module should wake.
    void deepSleepUntil(uint32_t untilTime);
    /// @brief Enters light sleep for the specified time.
    /// @param sleepTime The time in seconds to sleep.
    void lightSleep(float sleepTime);
    /// @brief Enters light sleep until the specified time.
    /// @param untilTime The time (UNIX epoch, seconds) the module should wake.
    void lightSleepUntil(uint32_t untilTime);

    /// @brief Gracefully shuts down the dependencies. This function can be thought of as a counterpoint to MIRRAModule::prepare.
    /// @see MIRRAModule::prepare
    void end();

    /// @brief Pin configuration in use by this module.
    const MIRRAPins pins;
    /// @brief RTC module in use by this module.
    PCF2129_RTC rtc;
    /// @brief LoRa module in use by this module.
    LoRaModule lora;

    /// @brief Flag that determines if command phase should be entered at startup.
    bool commandPhaseFlag{false};

    /// @brief Inline class that implements command functionality for a MIRRAModule T.
    /// @tparam T MIRRAModule to implement commands for.
    template <class T> class Commands
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

        /// @brief Creates the commands subsystem.
        /// @param parent Module for which this commands subsystem is implementing commands.
        Commands(T* parent) : parent{parent} {};

    public:
        /// @brief Checks the module's MIRRAModule::commandPhaseFlag and enters the command phase if it is set.
        void prompt();
    };
};

#include <MIRRAModule.tpp>

#endif
