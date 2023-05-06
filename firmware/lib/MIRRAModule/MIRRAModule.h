#ifndef __MIRRAMODULE_H__
#define __MIRRAMODULE_H__

#include <Arduino.h>
#include <CommunicationCommon.h>
#include "PCF2129_RTC.h"
#include <logging.h>
#include "LoRaModule.h"
#include <FS.h>
#include <SPIFFS.h>

#define LOG_FP "/"
#define LOG_LEVEL Logger::debug

#define UART_PHASE_ENTRY_PERIOD 15  // s, length of time after wakeup and comm period in which command phase can be entered
#define UART_PHASE_TIMEOUT (5 * 60) // s, length of UART inactivity required to automatically exit command phase

class MIRRAModule
{
public:
    struct MIRRAPins
    {
        uint8_t boot_pin;
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
    static MIRRAModule start(const MIRRAPins &pins);

    enum CommandCode : uint8_t
    {
        COMMAND_NOT_FOUND,
        COMMAND_FOUND,
        COMMAND_EXIT
    };
    void commandPhase();
    virtual CommandCode processCommands(char *command);

    void listFiles();
    void printFile(const char *filename, bool hex = false);

    void deepSleep(float time);
    void deepSleepUntil(uint32_t time);
    void lightSleep(float time);
    void lightSleepUntil(uint32_t time);

protected:
    static void prepare(const MIRRAPins &pins);
    MIRRAModule(const MIRRAPins &pins);

    const MIRRAPins pins;
    PCF2129_RTC rtc;
    Logger log;
    LoRaModule lora;
};

#endif
