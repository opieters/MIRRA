#include "LoRaModule.h"

LoRaModule::LoRaModule(PCF2129_RTC *rtc, Logger *log, const uint8_t csPin, const uint8_t rstPin, const uint8_t DIO0Pin, const uint8_t DIO1Pin, const uint8_t rxPin, const uint8_t txPin)
    : rtc{rtc},
      log{log},
      DIO0Pin{DIO0Pin},
      DIO1Pin{DIO1Pin},
      mod{Module(csPin, DIO0Pin, rstPin, DIO1Pin)},
      SX1272(&mod),
      lastSent{Message::error}
{
    pinMode(txPin, OUTPUT);
    pinMode(rxPin, OUTPUT);
    this->mod.setRfSwitchPins(rxPin, txPin);

    this->mac = MACAddress();
    esp_efuse_mac_get_default(this->mac.getAddress());

    int state = this->begin(LORA_FREQUENCY,
                            LORA_BANDWIDTH,
                            LORA_SPREADING_FACTOR,
                            LORA_CODING_RATE,
                            LORA_SYNC_WORD,
                            LORA_CURRENT_LIMIT,
                            LORA_PREAMBLE_LENGHT,
                            LORA_AMPLIFIER_GAIN);

    if (state == RADIOLIB_ERR_NONE)
    {
        log->print(Logger::info, "LoRa init successful!");
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

    uint8_t buffer[Message::max_length];
    int state = this->startTransmit(message.to_data(buffer), message.getLength());
    if (state == RADIOLIB_ERR_NONE)
    {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
        esp_light_sleep_start();
        log->print(Logger::debug, "Packet sent!");
        this->lastSent = message;
    }
    else
    {
        log->printf(Logger::error, "Send failed, code: %i", state);
    }
}

Message LoRaModule::receiveMessage(uint32_t timeout_ms, size_t repeat_attempts = 0, MACAddress repeat_mac = nullptr, bool promiscuous = false)
{
    if (repeat_mac == nullptr)
        repeat_mac = this->lastSent.getDest();
    uint8_t buffer[Message::max_length];

    // Set LoRa module in receive mode and start listening
    int state;
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // When the LoRa module get's a message it will generate an interrupt on DIO0.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
    // We use the timer wakeup as timeout for receiving a LoRa reply.
    esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
    do
    {
        state = this->startReceive();

        if (state != RADIOLIB_ERR_NONE)
        {
            log->printf(Logger::error, "Receive failed, code: %i", state);
            return Message::error;
        }

        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

        if (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO || wakeup_cause == ESP_SLEEP_WAKEUP_EXT0)
        {
            state = this->readData(buffer, min(this->getPacketLength(), Message::max_length));
            Message received = Message::from_data(buffer);

            if ((!promiscuous) && ((received.getDest() != this->mac) && (received.getDest() != MACAddress::broadcast)))
            {
                log->printf(Logger::debug, "Message from %s discarded because its destination does not match this device.", received.getDest().toString());
                continue;
            }
            if (received.isType(Message::Type::REPEAT))
            {
                log->printf(Logger::debug, "Received REPEAT message from %s", received.getSource().toString());
                if (this->lastSent.getDest() == received.getSource())
                {
                    this->sendMessage(this->lastSent);
                    esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
                }
                continue;
            }

            if (state == RADIOLIB_ERR_NONE)
            {
                return received;
            }
            else
            {
                log->printf(Logger::error, "Reading received data failed, code: %i", state);
                return Message::error;
            }
        }
        else
        {
            log->printf(Logger::debug, "Receive timeout after %ums", timeout_ms);
            if (repeat_attempts == 0)
            {
                return Message::error;
            }
            this->sendMessage(Message(Message::Type::REPEAT, this->mac, repeat_mac));
            esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
            repeat_attempts--;
        }

    } while (repeat_attempts >= 0);
    return Message::error;
}
