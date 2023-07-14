#include "LoRaModule.h"

LoRaModule::LoRaModule(const uint8_t csPin, const uint8_t rstPin, const uint8_t DIO0Pin, const uint8_t rxPin, const uint8_t txPin)
    : module{csPin, DIO0Pin, rstPin}, DIO0Pin{DIO0Pin}, SX1272(&module)
{
    this->module.setRfSwitchPins(rxPin, txPin);
    esp_efuse_mac_get_default(this->mac.getAddress());
    int state = this->begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNC_WORD, LORA_POWER, LORA_PREAMBLE_LENGHT,
                            LORA_AMPLIFIER_GAIN);
    if (state == RADIOLIB_ERR_NONE)
    {
        log->printf(Logger::debug, "LoRa init successful for %s!", this->getMACAddress().toString());
    }
    else
    {
        log->printf(Logger::error, "LoRa module init failed, code: %i", state);
    }
};

void LoRaModule::sendRepeat(const MACAddress& dest)
{
    log->printf(Logger::debug, "Sending REPEAT message to %s", dest.toString());
    auto repeatMessage = Message<REPEAT>(this->mac, dest);
    sendPacket(repeatMessage.toData(), repeatMessage.getLength());
}

void LoRaModule::sendPacket(uint8_t* buffer, size_t length)
{
    int state = this->startTransmit(buffer, length);
    if (state == RADIOLIB_ERR_NONE)
    {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
        esp_light_sleep_start();
        log->print(Logger::debug, "Packet sent!");
    }
    else
    {
        log->printf(Logger::error, "Send failed, code: %i", state);
    }
    this->finishTransmit();
}

void LoRaModule::resendPacket()
{
    if (sendLength == 0)
    {
        log->print(Logger::error, "Could not repeat last sent packet because no packet has been sent yet.");
        return;
    }
    log->printf(Logger::debug, "Resending last sent packet to %s", this->lastDest.toString());
    sendPacket(this->sendBuffer, this->sendLength);
}