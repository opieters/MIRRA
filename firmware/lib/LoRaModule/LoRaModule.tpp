#include <LoRaModule.h>
#include <algorithm>
#include <functional>

template <class T> void LoRaModule::sendMessage(T&& message, uint32_t delay)
{
    // When the transmission of a LoRa message is done an interrupt will be generated on DIO0,
    // this interrupt is used as wakeup source for the esp_light_sleep.
    char mac_src_buffer[MACAddress::stringLength];
    size_t length = message.getLength();
    Log::debug("Sending message of type ", message.getType(), " and length ", length, " from ", message.getSource().toString(mac_src_buffer), " to ",
               message.getDest().toString());
    this->sendLength = length;
    message.fromData(this->sendBuffer) = std::forward<T>(message);
    if (delay > 0)
    {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_sleep_enable_timer_wakeup(delay * 1000);
        esp_light_sleep_start();
    }
    sendPacket(this->sendBuffer, this->sendLength);
}

template <MessageType T>
std::optional<Message<T>> LoRaModule::receiveMessage(uint32_t timeoutMs, size_t repeatAttempts, const MACAddress& src, uint32_t listenMs, bool promiscuous)
{
    auto source{std::cref(src)};
    if (source.get() == MACAddress::broadcast && this->sendLength != 0)
        source = std::cref(this->getLastDest());
    timeoutMs /= repeatAttempts + 1;

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // When the LoRa module get's a message it will generate an interrupt on DIO0.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
    // We use the timer wakeup as timeout for receiving a LoRa reply.
    esp_sleep_enable_timer_wakeup((timeoutMs + listenMs) * 1000);
    do
    {
        Log::debug("Starting receive ...");
        int state{this->startReceive()};

        if (state != RADIOLIB_ERR_NONE)
        {
            Log::error("Receive failed, code: ", state);
            return std::nullopt;
        }

        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t wakeup_cause{esp_sleep_get_wakeup_cause()};

        if (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO || wakeup_cause == ESP_SLEEP_WAKEUP_EXT0)
        {
            uint8_t buffer[Message<T>::maxLength]{0};
            state = this->readData(buffer, std::min(this->getPacketLength(), Message<T>::maxLength));

            if (state == RADIOLIB_ERR_CRC_MISMATCH)
            {
                Log::error("Reading received data (", this->getPacketLength(false),
                           " bytes) failed because of a CRC mismatch. Waiting for timeout and possible sending of REPEAT...");
                continue;
            }
            if (state != RADIOLIB_ERR_NONE)
            {
                Log::error("Reading received data (", this->getPacketLength(false), " bytes) failed, code: ", state);
                return std::nullopt;
            }
            Log::debug("Reading received data (", this->getPacketLength(false), " bytes): success");
            Message<T>& received{Message<T>::fromData(buffer)};
            Log::debug("Message Type: ", received.getType());
            Log::debug("Source: ", received.getSource().toString());
            Log::debug("Dest: ", received.getDest().toString());
            if (source.get() != MACAddress::broadcast && source.get() != received.getSource())
            {
                char mac_src_buffer[MACAddress::stringLength];
                Log::debug("Message from ", received.getSource().toString(), " discared because it is not the desired source of the message, namely ",
                           source.get().toString(mac_src_buffer));
                continue;
            }

            if ((!promiscuous) && (received.getDest() != this->mac) && (received.getDest() != MACAddress::broadcast))
            {
                Log::debug("Message from ", received.getSource().toString(), " discarded because its destination does not match this device.");
                continue;
            }
            if (received.isType(REPEAT))
            {
                Log::debug("Received REPEAT message from ", received.getSource().toString());
                if (this->getLastDest() == received.getSource())
                {
                    this->resendMessage();
                    esp_sleep_enable_timer_wakeup(timeoutMs * 1000);
                }
                continue;
            }
            if (!received.isValid())
            {
                Log::debug("Message of type ", received.getType(), " discarded because message of type ", T, " is desired.");
                continue;
            }

            return received;
        }
        else
        {
            Log::debug("Receive timeout after ", timeoutMs, "ms with ", repeatAttempts, " repeat attempts left.");
            if (repeatAttempts == 0)
            {
                return std::nullopt;
            }
            this->sendRepeat(source);
            esp_sleep_enable_timer_wakeup(timeoutMs * 1000);
            repeatAttempts--;
        }

    } while (repeatAttempts >= 0);
    return std::nullopt;
}