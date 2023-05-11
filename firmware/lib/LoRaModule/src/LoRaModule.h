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
#define LORA_CODING_RATE 6
#define LORA_SYNC_WORD 0x12
#define LORA_POWER 10
#define LORA_PREAMBLE_LENGHT 8
#define LORA_AMPLIFIER_GAIN 0 // 0 is automatic

class LoRaModule : public SX1272
{
private:
    Module mod;

    Logger *log;

    MACAddress mac;

    const uint8_t DIO0Pin;
    const uint8_t DIO1Pin;

    Message lastSent;

public:
    LoRaModule(Logger *log, const uint8_t csPin, const uint8_t rstPin,
               const uint8_t DIOPin, const uint8_t rxPin, const uint8_t txPin);

    MACAddress getMACAddress(void) { return mac; };

    Message receiveMessage(uint32_t timeout_ms, Message::Type type = Message::Type::ALL, size_t repeat_attempts = 0, MACAddress source = MACAddress::broadcast, uint32_t listen_ms = 0, bool promiscuous = false);
    void sendMessage(Message message);
};

#endif
