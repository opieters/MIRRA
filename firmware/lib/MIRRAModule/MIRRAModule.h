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

class MIRRAModule
{
public:
    struct MIRRAPins
    {
        uint8_t boot_pin;
        uint8_t per_power_pin;
        uint8_t sda_pin;
        uint8_t scl_pin;
        uint8_t cs_pin;
        uint8_t rst_pin;
        uint8_t dio0_pin;
        uint8_t tx_pin;
        uint8_t rx_pin;
        uint8_t rtc_int_pin;
        uint8_t rtc_address;
    };
    static void prepare(const MIRRAPins& pins);

protected:
    MIRRAModule(const MIRRAPins& pins);
    void storeSensorData(Message<SENSOR_DATA>& m, File& dataFile);
    void pruneSensorData(File&& dataFile, uint32_t maxSize);

    void deepSleep(uint32_t time);
    void deepSleepUntil(uint32_t untilTime);
    void lightSleep(float time);
    void lightSleepUntil(uint32_t untilTime);

    void end();

    const MIRRAPins pins;
    PCF2129_RTC rtc;
    LoRaModule lora;

    bool commandPhaseFlag{false};

    template <class T> class Commands
    {
    protected:
        static const size_t lineMaxLength{256};

        enum CommandCode : uint8_t
        {
            COMMAND_NOT_FOUND,
            COMMAND_FOUND,
            COMMAND_EXIT
        };

        T* parent;

        void start();
        std::optional<std::array<char, lineMaxLength>> readLine();
        CommandCode processCommands(char* command);

        void listFiles();
        void printFile(const char* filename, bool hex = false);
        void removeFile(const char* filename);
        void touchFile(const char* filename);

        Commands(T* parent) : parent{parent} {};

    public:
        void prompt();
    };
};

#include <MIRRAModule.tpp>

#endif
