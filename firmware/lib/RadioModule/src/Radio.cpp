#include "Radio.h"

void printArray(uint8_t* data, size_t len){
    size_t i;

    Serial.print("[");
    for(i = 0; i < len; i++){
        Serial.print(data[i], HEX);
        if(i != (len-1)){
            Serial.print(" ");
        }
    }
    Serial.print("]");
}

#define SECONDS_TO_US(X) ((X)*1000000)

// internal function
bool arrayCompare(uint8_t arr1[], uint8_t arr2[], size_t size1, size_t size2){
    // If the 2 arrays are not the same size we can already return false
    if(size1 != size2) 
        return false;
    
    for(size_t i = 0; i < size1; ++i)
    {
        if(arr1[i] != arr2[i])
        {
            return false;
        }
    }
    return true;
}


RadioModule::RadioModule(PCF2129_RTC* rtc, const uint8_t ssPin, const uint8_t rstPin,
        const uint8_t DIOPin, const uint8_t DI1Pin, const uint8_t rxPin, const uint8_t txPin): ssPin(ssPin), rstPin(rstPin), DIOPin(DIOPin), DI1Pin(DI1Pin), rxPin(rxPin), txPin(txPin) {
    this->communication_module = LoRa(ssPin, DIOPin, DI1Pin);
    this->lora_mod = SX1272(&communication_module);
    this->rtc = rtc;

    pinMode(txPin, OUTPUT);
    pinMode(rxPin, OUTPUT);

    esp_efuse_mac_get_default(this->macAddress);
};

RadioModule::~RadioModule() {};

void RadioModule::init() {
    int state = lora_mod.begin(LORA_FREQUENCY, 
                                LORA_BANDWIDTH,
                                LORA_SPREADING_FACTOR,
                                LORA_CODING_RATE,
                                LORA_SYNC_WORD,
                                LORA_POWER,
                                LORA_CURRENT_LIMIT,
                                LORA_PREAMBLE_LENGHT,
                                LORA_AMPLIFIER_GAIN);

    if (state == ERR_NONE) {
        Serial.println("LoRa init successful!");
    } else {
        Serial.print("LoRa module init failed, code: ");
        Serial.println(state);
    }
};

void RadioModule::sleep() {
    lora_mod.sleep();
}


void RadioModule::sendMessage(LoRaMessage& message){
    digitalWrite(this->rxPin, LOW);
    digitalWrite(this->txPin, HIGH);

    // When the transmission of the LoRa message is done an interrupt will be generated on DIO0,
    // this interrupt is used as wakeup source for the esp_light_sleep.
    esp_sleep_enable_ext0_wakeup((gpio_num_t) this->DIOPin, 1);

    Serial.print("Sending ");
    printArray(message.getData(), message.getLength());
    Serial.println();
    
    int state = lora_mod.startTransmit(message.getData(), message.getLength());
    if (state == ERR_NONE) {
        Serial.println("Packet sent!");
        esp_light_sleep_start();
    } else {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
        Serial.print("Send failed, code: ");
        Serial.println(state);
    }
}

bool RadioModule::receiveAnyMessage(uint32_t timeout_ms, LoRaMessage& message){
    uint8_t receiveBuffer[MAX_MESSAGE_LENGTH];

    digitalWrite(this->rxPin, HIGH);
    digitalWrite(this->txPin, LOW);

        // Set LoRa module in receive mode and start listening
    int state = lora_mod.startReceive();
    
    if (state == ERR_NONE) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        // When the LoRa module get's a message it will generate an interrupt on DIO0.
        esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIOPin, 1);
        // We use the timer wakeup as timeout for receiving a LoRa reply.
        esp_sleep_enable_timer_wakeup(SECONDS_TO_US(LORA_RECEIVE_TIMEOUT));

        esp_light_sleep_start();
        
        // If the GPIO is the wakup source this means that we received a message and we didn't timeout
        esp_sleep_wakeup_cause_t wakup_cause = esp_sleep_get_wakeup_cause();
        //Serial.print(F("Wakeup a result of"));
        //Serial.println(wakup_cause);

        if (wakup_cause == ESP_SLEEP_WAKEUP_GPIO || wakup_cause == ESP_SLEEP_WAKEUP_EXT0) {
            size_t length = lora_mod.getPacketLength();
            if(length > MAX_MESSAGE_LENGTH){
                length = MAX_MESSAGE_LENGTH;
            }
            state = lora_mod.readData(receiveBuffer, length);
            message.setData(receiveBuffer, length);

            if (state == ERR_NONE) {
                // Let's print the message we received.
                Serial.print("Received ");
                printArray(message.getData(), message.getLength());
                Serial.println();
                // print RSSI (Received Signal Strength Indicator)
                /*Serial.println(F("RSSI:\t\t\t"));
                Serial.print(lora_mod.getRSSI());
                Serial.println(F(" dBm"));

                // print SNR (Signal-to-Noise Ratio)
                Serial.print(F("SNR:\t\t\t"));
                Serial.print(lora_mod.getSNR());
                Serial.println(F(" dB"));

                // print frequency error
                Serial.print(F("Frequency error:\t"));
                Serial.print(lora_mod.getFrequencyError());
                Serial.println(F(" Hz"));*/
                return true;
            } else {
                Serial.print("Error while receiving data, error code: ");
                Serial.println(state);
                return false;
            }
        } else {
            //Serial.println("No reply received.");
            return false;
        }
    } else {
        Serial.print(F("Reading failed, code: "));
        Serial.println(state);
        return false;
    }
}

bool RadioModule::receiveMessage(uint32_t timeout_ms, LoRaMessage& message){
    bool status;
    uint32_t timeout = rtc->read_time_epoch() + (timeout_ms+500) / 1000;
    do{
        status = receiveAnyMessage(timeout_ms, message);
        if(status && checkRecipient(message)){
            return true;
        } else {
            status = false;
        }
    } while(!status && (rtc->read_time_epoch() < timeout));
    return false;
}

bool RadioModule::receiveMessage(uint32_t timeout_ms, LoRaMessage& message, uint8_t* macAddress){
    bool status;
    uint32_t timeout = rtc->read_time_epoch() + (timeout_ms+500) / 1000;
    do{
        status = receiveAnyMessage(timeout_ms, message);
        if(status && checkRecipient(message, macAddress)){
            return true;
        } else {
            status = false;
        }
    } while(!status && (rtc->read_time_epoch() < timeout));
    return false;
}

bool RadioModule::receiveMessage(uint32_t timeout_ms, LoRaMessage& message, uint8_t* macAddress, CommunicationCommand cmd){
    bool status;
    uint32_t timeout = rtc->read_time_epoch() + (timeout_ms+500) / 1000;
    do{
        status = receiveMessage(timeout_ms, message, macAddress);
        if(status && (message.getLength() >= 7) && (message.getData()[6] == CommunicationCommandToInt(cmd))){
            return true;
        } else {
            status = false;
        }
    } while(!status && (rtc->read_time_epoch() < timeout));
    return false;
}

bool RadioModule::checkRecipient(LoRaMessage& message){
    size_t length = message.getLength();
    if(length >= 6){
        length = 6;
    }
    return arrayCompare(macAddress, message.getData(), 6, length);
}

bool RadioModule::checkRecipient(LoRaMessage& message, uint8_t* macAddress){
    size_t length = message.getLength();
    if(length >= 6){
        length = 6;
    }
    return arrayCompare(macAddress, message.getData(), 6, length);
}

bool RadioModule::receiveSpecificMessage(uint32_t timeout_ms, LoRaMessage& message, CommunicationCommand cmd){
    bool status;
    uint32_t timeout = rtc->read_time_epoch() + (timeout_ms+500) / 1000;
    do{
        status = receiveMessage(timeout_ms, message);
        if(status && (message.getLength() >= 7) && (message.getData()[6] == CommunicationCommandToInt(cmd))){
            return true;
        } else {
            status = false;
        }
    } while(!status && (rtc->read_time_epoch() < timeout));
    return false;
}


LoRaMessage::LoRaMessage(uint8_t* data, size_t length){
    size_t i;

    length = min(length, LoRaMessage::max_length);

    for(i = 0; i < length; i++){
        this->data[i] = data[i];
    }
    this->length = length;
}

void LoRaMessage::setData(uint8_t* data, size_t length){
    size_t i;
    
    this->length = min(length, LoRaMessage::max_length);;
    for(i = 0; i < this->length; i++){
        this->data[i] = data[i];
    }
}
