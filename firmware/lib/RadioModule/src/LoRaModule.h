#ifndef __RADIO_H__
#define __RADIO_H__

#include <Arduino.h>
#include <RadioLib.h>
#include <PCF2129_RTC.h>
#include <CommunicationCommon.h>
#include <logging.h>

/******************************
 *     LoRa configuration
 *****************************/
#define LORA_FREQUENCY 866.0
#define LORA_BANDWIDTH 125.0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0x12
#define LORA_POWER 5
#define LORA_CURRENT_LIMIT 100
#define LORA_PREAMBLE_LENGHT 8
#define LORA_AMPLIFIER_GAIN 0 // 0 is automatic

class LoRaModule : SX1272
{
private:
    Module mod;

    MACAddress macAddress;

    PCF2129_RTC *rtc;
    Logger *log;

    const uint8_t DIO0Pin;
    const uint8_t DIO1Pin;

    Message lastSent;

public:
    LoRaModule(PCF2129_RTC *rtc, Logger *log, const uint8_t ssPin, const uint8_t rstPin,
               const uint8_t DIOPin, const uint8_t DI1Pin, const uint8_t rxPin, const uint8_t txPin);
    ~LoRaModule();

    void init();

    MACAddress getMACAddress(void) { return macAddress; };

    Message receiveMessage(uint32_t timeout_ms, size_t repeat_attempts = 0, MACAddress repeat_mac = nullptr, bool promiscuous = false);
    void sendMessage(Message message);
};

#endif
