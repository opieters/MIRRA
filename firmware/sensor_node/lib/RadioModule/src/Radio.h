#ifndef __RADIO_H__
#define __RADIO_H__

#include <Arduino.h>
#include <LoRaLib.h>
#include <PCF2129_RTC.h>
#include <CommunicationCommon.h>

#define MAC_NUM_BYTES 6

// LoRa module
#define MODULE_SX1272

// Pin assignments



    /******************************
 *     LoRa configuration
 *****************************/
#define LORA_FREQUENCY 866.0
#define LORA_BANDWIDTH 125.0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0x12
#define LORA_POWER 10
#define LORA_CURRENT_LIMIT 100
#define LORA_PREAMBLE_LENGHT 8
#define LORA_AMPLIFIER_GAIN 0 // 0 is automatic

// Receive timeout while waiting a reply from the gateway, in seconds.
#define LORA_RECEIVE_TIMEOUT 5

#define MAX_MESSAGE_LENGTH 256

class LoRaMessage {
    private:
        uint8_t data[256];
        size_t length;
    public:
        LoRaMessage(uint8_t* data = nullptr, size_t length = 0);

        uint8_t* getData() { return data; };
        size_t getLength() { return length; };
        void setData(uint8_t* data, size_t length);

        static const size_t max_length = 256;
};


class RadioModule {
private:
    Module communication_module;
    SX1272 lora_mod = SX1272(&communication_module);

    static bool receive_flag;
    static void set_receive_flag();

    uint8_t macAddress[MAC_NUM_BYTES];
    
    PCF2129_RTC* rtc;

    const uint8_t ssPin;
    const uint8_t rstPin;
    const uint8_t DIOPin;
    const uint8_t DI1Pin;
    const uint8_t rxPin;
    const uint8_t txPin;

public:
    RadioModule(PCF2129_RTC* rtc, const uint8_t ssPin, const uint8_t rstPin,
        const uint8_t DIOPin, const uint8_t DI1Pin, const uint8_t rxPin, const uint8_t txPin);
    ~RadioModule();

    void init(void);

    bool searchGateway(void);

    bool sendMessage(uint8_t* message, size_t length);

    void sleep(void);

    uint8_t* getMACAddress(void) { return macAddress; };

    bool receiveAnyMessage(uint32_t timeout_ms, LoRaMessage& message);
    bool receiveMessage(uint32_t timeout_ms, LoRaMessage& message);
    bool receiveMessage(uint32_t timeout_ms, LoRaMessage& message, uint8_t* macAddress);
    bool receiveMessage(uint32_t timeout_ms, LoRaMessage& message, uint8_t* macAddress, CommunicationCommand cmd);
    void sendMessage(LoRaMessage& message);

    bool checkRecipient(LoRaMessage& message);
    bool checkRecipient(LoRaMessage& message, uint8_t* macAddress);

    bool receiveSpecificMessage(uint32_t timeout_ms, LoRaMessage& message, CommunicationCommand cmd);
};

#endif
