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
#define LORA_CODING_RATE 8
#define LORA_SYNC_WORD 0x12
#define LORA_POWER 10
#define LORA_PREAMBLE_LENGHT 8
#define LORA_AMPLIFIER_GAIN 0 // 0 is automatic

class LoRaModule : public SX1272
{
private:
    Module module;
    Logger *log;

    MACAddress mac;

    const uint8_t DIO0Pin;
    const uint8_t DIO1Pin;

    MACAddress lastDest;
    uint8_t lastSentBuffer[Message::max_length]{0};
    size_t lastSentLength = 0;

public:
    LoRaModule(Logger *log, const uint8_t csPin, const uint8_t rstPin,
               const uint8_t DIOPin, const uint8_t rxPin, const uint8_t txPin);

    MACAddress getMACAddress(void) { return mac; };

    template <class MessageType>
    void sendMessage(MessageType const &message);
    void sendPacket(uint8_t *buffer, size_t length);
    void resendPacket();

    template <class MessageType>
    MessageType receiveMessage(uint32_t timeout_ms, Message::Type type = Message::Type::ALL, size_t repeat_attempts = 0, MACAddress source = MACAddress::broadcast, uint32_t listen_ms = 0, bool promiscuous = false);
};

template <class MessageType>
void LoRaModule::sendMessage(MessageType const &message)
{

    // When the transmission of the LoRa message is done an interrupt will be generated on DIO0,
    // this interrupt is used as wakeup source for the esp_light_sleep.
    char mac_src_buffer[MACAddress::string_length];
    size_t length = message.getLength();
    log->printf(Logger::debug, "Sending message of type %u and length %u from %s to %s", message.getType(), length, message.getSource().toString(mac_src_buffer), message.getDest().toString());
    uint8_t buffer[length];
    if (!message.isType(Message::Type::REPEAT))
        memcpy(this->lastSentBuffer, buffer, length);
    this->lastSentLength = length;
    this->lastDest = message.getDest();
    sendPacket(buffer, length);
}

template <class MessageType>
MessageType LoRaModule::receiveMessage(uint32_t timeout_ms, Message::Type type, size_t repeat_attempts, MACAddress source, uint32_t listen_ms, bool promiscuous)
{
    if (source == MACAddress::broadcast && this->lastSentLength != 0)
    {
        source = this->lastDest;
    }
    timeout_ms /= repeat_attempts + 1;

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // When the LoRa module get's a message it will generate an interrupt on DIO0.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
    // We use the timer wakeup as timeout for receiving a LoRa reply.
    esp_sleep_enable_timer_wakeup((timeout_ms + listen_ms) * 1000);
    do
    {
        log->printf(Logger::debug, "Starting receive ...");
        int state = this->startReceive();

        if (state != RADIOLIB_ERR_NONE)
        {
            log->printf(Logger::error, "Receive failed, code: %i", state);
            return MessageType();
        }

        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

        if (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO || wakeup_cause == ESP_SLEEP_WAKEUP_EXT0)
        {
            uint8_t buffer[Message::max_length];
            state = this->readData(buffer, min(this->getPacketLength(), Message::max_length));

            if (state == RADIOLIB_ERR_CRC_MISMATCH)
            {
                log->printf(Logger::error, "Reading received data (%u bytes) failed because of a CRC mismatch. Waiting for timeout and possible sending of REPEAT...", this->getPacketLength(false));
                continue;
            }
            if (state != RADIOLIB_ERR_NONE)
            {
                log->printf(Logger::error, "Reading received data (%u bytes) failed, code: %i", this->getPacketLength(false), state);
                return MessageType();
            }

            MessageType received = MessageType(buffer);

            if (source != nullptr && source != received.getSource())
            {
                log->printf(Logger::debug, "Message from %s discared because it is not the desired source of the message", received.getSource().toString());
                continue;
            }

            if ((!promiscuous) && (received.getDest() != this->mac) && (received.getDest() != MACAddress::broadcast))
            {
                log->printf(Logger::debug, "Message from %s discarded because its destination does not match this device.", received.getDest().toString());
                continue;
            }
            if (received.isType(Message::Type::REPEAT))
            {
                log->printf(Logger::debug, "Received REPEAT message from %s", received.getSource().toString());
                if (this->lastDest == received.getSource())
                {
                    this->resendPacket();
                    esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
                    repeat_attempts--;
                }
                continue;
            }
            if (type != Message::Type::ALL && !received.isType(type))
            {
                log->printf(Logger::debug, "Message of type %u discarded because message of type %u is desired.", received.getType(), type);
                continue;
            }

            return received;
        }
        else
        {
            log->printf(Logger::debug, "Receive timeout after %ums with %u repeat attempts left.", timeout_ms, repeat_attempts);
            if (repeat_attempts == 0)
            {
                return MessageType();
            }
            log->printf(Logger::debug, "Sending REPEAT message...");
            this->sendMessage(Message(Message::Type::REPEAT, this->mac, source));
            esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
            repeat_attempts--;
        }

    } while (repeat_attempts >= 0);
    return MessageType();
}

#endif
