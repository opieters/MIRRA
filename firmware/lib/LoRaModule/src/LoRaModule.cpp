#include "LoRaModule.h"

LoRaModule::LoRaModule(Logger *log, const uint8_t csPin, const uint8_t rstPin, const uint8_t DIO0Pin, const uint8_t rxPin, const uint8_t txPin)
    : log{log},
      DIO0Pin{DIO0Pin},
      DIO1Pin{DIO1Pin},
      module{Module(csPin, DIO0Pin, rstPin)},
      SX1272(&module),
      lastSent{Message()}
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