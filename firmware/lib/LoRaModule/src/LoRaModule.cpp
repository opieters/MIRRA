#include "LoRaModule.h"

LoRaModule::LoRaModule(Logger *log, const uint8_t csPin, const uint8_t rstPin, const uint8_t DIO0Pin, const uint8_t rxPin, const uint8_t txPin)
    : log{log},
      DIO0Pin{DIO0Pin},
      DIO1Pin{DIO1Pin},
      module{Module(csPin, DIO0Pin, rstPin)},
      SX1272(&module),
      lastSent{Message::error}
{
    this->reset();
    this->explicitHeader();
    this->module.setRfSwitchPins(rxPin, txPin);

    this->mac = MACAddress();
    esp_efuse_mac_get_default(this->mac.getAddress());
    int state = this->begin(LORA_FREQUENCY,
                            LORA_BANDWIDTH,
                            LORA_SPREADING_FACTOR,
                            LORA_CODING_RATE,
                            LORA_SYNC_WORD,
                            LORA_POWER,
                            LORA_PREAMBLE_LENGHT,
                            LORA_AMPLIFIER_GAIN);
    if (state == RADIOLIB_ERR_NONE)
    {
        log->printf(Logger::info, "LoRa init successful for %s!", this->getMACAddress().toString());
    }
    else
    {
        log->printf(Logger::error, "LoRa module init failed, code: %i", state);
    }
};

void LoRaModule::sendMessage(Message message)
{

    // When the transmission of the LoRa message is done an interrupt will be generated on DIO0,
    // this interrupt is used as wakeup source for the esp_light_sleep.
    char mac_src_buffer[MACAddress::string_length];
    log->printf(Logger::debug, "Sending message of type %u from %s to %s", message.getType(), message.getSource().toString(mac_src_buffer), message.getDest().toString());
    uint8_t buffer[Message::max_length];
    int state = this->startTransmit(message.to_data(buffer), message.getLength());
    if (state == RADIOLIB_ERR_NONE)
    {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
        esp_light_sleep_start();
        log->print(Logger::debug, "Packet sent!");
        if (!message.isType(Message::Type::REPEAT))
            this->lastSent = message;
    }
    else
    {
        log->printf(Logger::error, "Send failed, code: %i", state);
    }
    this->finishTransmit();
}

Message LoRaModule::receiveMessage(uint32_t timeout_ms, Message::Type type, size_t repeat_attempts, MACAddress source, uint32_t listen_ms, bool promiscuous)
{
    if (source == MACAddress::broadcast && this->lastSent.getType() != Message::ERROR)
    {
        source = this->lastSent.getDest();
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
            return Message::error;
        }

        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

        if (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO || wakeup_cause == ESP_SLEEP_WAKEUP_EXT0)
        {
            uint8_t buffer[Message::max_length];
            state = this->readData(buffer, min(this->getPacketLength(), Message::max_length));

            if (state != RADIOLIB_ERR_NONE)
            {
                log->printf(Logger::error, "Reading received data (%u bytes) failed, code: %i", this->getPacketLength(), state);
                return Message::error;
            }

            Message received = Message::from_data(buffer);

            if (received.isType(Message::Type::REPEAT))
            {
                log->printf(Logger::debug, "Received REPEAT message from %s", received.getSource().toString());
                if (this->lastSent.getDest() == received.getSource())
                {
                    this->sendMessage(this->lastSent);
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

            return received;
        }
        else
        {
            log->printf(Logger::debug, "Receive timeout after %ums with %u repeat attempts left.", timeout_ms, repeat_attempts);
            if (repeat_attempts == 0)
            {
                return Message::error;
            }
            log->printf(Logger::debug, "Sending REPEAT message...");
            this->sendMessage(Message(Message::Type::REPEAT, this->mac, source));
            esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
            repeat_attempts--;
        }

    } while (repeat_attempts >= 0);
    return Message::error;
}
