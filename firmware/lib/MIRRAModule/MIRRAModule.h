#ifndef __MIRRAMODULE_H__
#define __MIRRAMODULE_H__

#include "Commands.h"
#include "CommunicationCommon.h"
#include "LoRaModule.h"
#include "PCF2129_RTC.h"
#include "logging.h"
#include <Arduino.h>
#include <LittleFS.h>

#define LOG_LEVEL Log::INFO

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
    /// @brief Stores the given sensor data message into the module's flash filesystem. The first type byte of the message is replaced with an 'upload' flag, at
    /// first set to 0.
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

    CommandEntry commandEntry;
};

#endif
