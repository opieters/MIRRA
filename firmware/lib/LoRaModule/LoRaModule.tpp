#include <LoRaModule.h>
#include <algorithm>
#include <functional>

template <class T> void LoRaModule::sendMessage(T&& message, uint32_t delay)
{
    // When the transmission of the LoRa message is done an interrupt will be generated on DIO0,
    // this interrupt is used as wakeup source for the esp_light_sleep.
    char mac_src_buffer[MACAddress::string_length];
    size_t length = message.getLength();
    log->printf(Logger::debug, "Sending message of type %u and length %u from %s to %s", message.getType(), length,
                message.getSource().toString(mac_src_buffer), message.getDest().toString());
    this->sendLength = length;
    message.fromData(this->sendBuffer) = std::forward<T>(message);
    this->lastDest = message.getDest();
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
    {
        source = std::cref(this->lastDest);
    }
    timeoutMs /= repeatAttempts + 1;

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // When the LoRa module get's a message it will generate an interrupt on DIO0.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)this->DIO0Pin, 1);
    // We use the timer wakeup as timeout for receiving a LoRa reply.
    esp_sleep_enable_timer_wakeup((timeoutMs + listenMs) * 1000);
    do
    {
        log->printf(Logger::debug, "Starting receive ...");
        int state{this->startReceive()};

        if (state != RADIOLIB_ERR_NONE)
        {
            log->printf(Logger::error, "Receive failed, code: %i", state);
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
                log->printf(Logger::error,
                            "Reading received data (%u bytes) failed because of a CRC mismatch. Waiting for timeout and possible sending of REPEAT...",
                            this->getPacketLength(false));
                continue;
            }
            if (state != RADIOLIB_ERR_NONE)
            {
                log->printf(Logger::error, "Reading received data (%u bytes) failed, code: %i", this->getPacketLength(false), state);
                return std::nullopt;
            }
            log->printf(Logger::debug, "Reading received data (%u bytes): success", this->getPacketLength(false));
            Message<T>& received{Message<T>::fromData(buffer)};
            log->printf(Logger::debug, "Message Type: %u", received.getType());
            log->printf(Logger::debug, "Source: %s", received.getSource().toString());
            log->printf(Logger::debug, "Dest: %s", received.getDest().toString());
            if (source.get() != MACAddress::broadcast && source.get() != received.getSource())
            {
                char mac_src_buffer[MACAddress::string_length];
                log->printf(Logger::debug, "Message from %s discared because it is not the desired source of the message, namely %s",
                            received.getSource().toString(), source.get().toString(mac_src_buffer));
                continue;
            }

            if ((!promiscuous) && (received.getDest() != this->mac) && (received.getDest() != MACAddress::broadcast))
            {
                log->printf(Logger::debug, "Message from %s discarded because its destination does not match this device.", received.getSource().toString());
                continue;
            }
            if (received.isType(REPEAT))
            {
                log->printf(Logger::debug, "Received REPEAT message from %s", received.getSource().toString());
                if (this->lastDest == received.getSource())
                {
                    this->resendPacket();
                    esp_sleep_enable_timer_wakeup(timeoutMs * 1000);
                }
                continue;
            }
            if (!received.isValid())
            {
                log->printf(Logger::debug, "Message of type %u discarded because message of type %u is desired.", received.getType(), received.desiredType);
                continue;
            }

            return received;
        }
        else
        {
            log->printf(Logger::debug, "Receive timeout after %ums with %u repeat attempts left.", timeoutMs, repeatAttempts);
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