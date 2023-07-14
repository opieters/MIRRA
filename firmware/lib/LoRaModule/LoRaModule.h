#ifndef __RADIO_H__
#define __RADIO_H__

#include <Arduino.h>
#include <CommunicationCommon.h>
#include <PCF2129_RTC.h>
#include <RadioLib.h>
#include <logging.h>

#include <optional>

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

#define SEND_DELAY 500 // ms, time to wait before sending a message

class LoRaModule : public SX1272
{
private:
    Module module;

    MACAddress mac{};

    const uint8_t DIO0Pin;

    uint8_t sendBuffer[MessageHeader::maxLength]{0};
    size_t sendLength{0};
    MACAddress lastDest{};

public:
    LoRaModule(const uint8_t csPin, const uint8_t rstPin, const uint8_t DIOPin, const uint8_t rxPin, const uint8_t txPin);

    const MACAddress& getMACAddress(void) { return mac; };

    template <class T> void sendMessage(T&& message, uint32_t delay = SEND_DELAY);
    void sendRepeat(const MACAddress& dest);
    void sendPacket(uint8_t* buffer, size_t length);
    void resendPacket();

    template <MessageType T>
    std::optional<Message<T>> receiveMessage(uint32_t timeoutMs, size_t repeatAttempts = 0, const MACAddress& src = MACAddress::broadcast,
                                             uint32_t listenMs = 0, bool promiscuous = false);
};

#include <LoRaModule.tpp>

#endif
