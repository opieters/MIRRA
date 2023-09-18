#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <array>

#include "Sensor.h"

/// @brief Wrapper around std::array that provides an interface for MAC address operations.
class MACAddress
{
private:
    std::array<uint8_t, 6> address{ 0 };

public:
    /// @brief Constructs an empty (i.e. all bytes 0) MACAddress.
    MACAddress() = default;
    /// @brief Construct a MACAddress from a raw byte pointer.
    /// @param address Raw byte pointer to MACAddress.
    MACAddress(const uint8_t* address)
      : address{ *reinterpret_cast<const std::array<uint8_t, 6>*>(address) }
    {
    }
    /// @brief Gets the raw byte pointer to the MAC address.
    /// @return A raw byte pointer to the MAC address.
    uint8_t* getAddress() { return address.data(); }
    /// @brief Gives a hex string representation of this MAC address.
    /// @param string Buffer to write the resulting string to. By default, this uses a static buffer.
    /// @return The buffer to which the string was written.
    char* toString(char* string = strBuffer) const;
    bool operator==(const MACAddress& other) const { return this->address == other.address; }
    bool operator!=(const MACAddress& other) const { return this->address != other.address; }

    /// @brief Constructs a MAC address from a string.
    /// @param string The hex string from which to construct the MAC address in the 00:00:00:00:00:00 format.
    /// @return The constructed MAC address.
    static MACAddress fromString(char* string);

    /// @brief Size of the MAC address in bytes.
    static constexpr size_t length = sizeof(address);
    /// @brief Size of the string representation of a MAC address in bytes, including terminator.
    static constexpr size_t stringLength = length * 3;
    /// @brief Broadcast MAC address.
    static const MACAddress broadcast;

private:
    /// @brief Internal static string buffer used by the toString method.
    static char strBuffer[stringLength];
} __attribute__((packed));

/// @brief Enum used to indicate the type of a message. Maximum of 128 available types.
enum MessageType : uint8_t
{
    ERROR = 0,
    HELLO = 1,
    HELLO_REPLY = 2,
    TIME_CONFIG = 3,
    ACK_TIME = 4,
    SENSOR_DATA = 5,
    ACK_DATA = 6,
    REPEAT = 7,
    ALL = 8
};

/// @brief Base class providing a common interface between all message types and the header portion of the message.
class MessageHeader
{
private:
    /// @brief Type of the message
    MessageType type : 7;
    /// @brief Last flag
    bool last : 1;
    /// @brief The source MAC address of the message.
    MACAddress src{};
    /// @brief The destination MAC address of the message.
    MACAddress dest{};

protected:
    MessageHeader(MessageType type, const MACAddress& src, const MACAddress& dest)
      : type{ type }
      , last{ false }
      , src{ src }
      , dest{ dest }
    {
    }

public:
    constexpr MessageType getType() const { return this->type; }
    constexpr bool isType(MessageType type) const { return this->type == type; }
    constexpr bool isLast() const { return last; }
    constexpr const MACAddress& getSource() const { return src; }
    constexpr const MACAddress& getDest() const { return dest; }

    /// @brief Forcibly sets the type of this message.
    /// @param type The type to force-set this message to.
    void setType(MessageType type) { this->type = type; }
    /// @brief Sets the destination for this message.
    void setDest(const MACAddress& dest) { this->dest = dest; }
    /// @brief Sets the LAST flag of this message.
    void setLast(bool last = true) { this->last = last; }

    /// @brief Converts this message in-place to a byte buffer.
    /// @return The pointer to the resulting byte buffer.
    constexpr const uint8_t* toData() const { return reinterpret_cast<const uint8_t*>(this); }

    /// @brief The length of the header in bytes.
    static constexpr size_t headerLength{ 1 + 2 * sizeof(MACAddress) };
    /// @brief The maximum length of a message in bytes.
    static constexpr size_t maxLength{ 256 };
} __attribute__((packed));

constexpr size_t test{ sizeof(MessageHeader) };

/// @brief Final message class.
/// @tparam T The message type of the message.
template<MessageType T>
class Message : public MessageHeader
{
public:
    Message(const MACAddress& src, const MACAddress& dest)
      : MessageHeader(T, src, dest){};
    /// @return The messages' length in bytes.
    constexpr size_t getLength() const { return headerLength; }
    /// @return Whether the message's type flag matches the desired type.
    constexpr bool isValid() const { return isType(T); }
    /// @brief Converts a byte buffer in-place to this message type, without any runtime checking.
    /// @param data The byte buffer to interpret a message from.
    /// @return The resulting message object.
    constexpr static Message<T>& fromData(uint8_t* data) { return *reinterpret_cast<Message<T>*>(data); }
} __attribute__((packed));

template<>
class Message<TIME_CONFIG> : public MessageHeader
{
private:
    uint32_t curTime, sampleInterval, sampleRounding, sampleOffset, commInterval, commTime, maxMessages;

public:
    Message(const MACAddress& src,
            const MACAddress& dest,
            uint32_t curTime,
            uint32_t sampleInterval,
            uint32_t sampleRounding,
            uint32_t sampleOffset,
            uint32_t commInterval,
            uint32_t commTime,
            uint32_t maxMessages)
      : MessageHeader(TIME_CONFIG, src, dest)
      , curTime{ curTime }
      , sampleInterval{ sampleInterval }
      , sampleRounding{ sampleRounding }
      , sampleOffset{ sampleOffset }
      , commInterval{ commInterval }
      , commTime{ commTime }
      , maxMessages{ maxMessages } {};

    uint32_t getCTime() const { return curTime; };
    uint32_t getSampleInterval() const { return sampleInterval; };
    uint32_t getSampleRounding() const { return sampleRounding; };
    uint32_t getSampleOffset() const { return sampleOffset; };
    uint32_t getCommInterval() const { return commInterval; };
    uint32_t getCommTime() const { return commTime; };
    uint32_t getMaxMessages() const { return maxMessages; };

    /// @return The messages' length in bytes.
    constexpr size_t getLength() const { return sizeof(*this); };
    /// @return Whether the message's type flag matches the desired type.
    constexpr bool isValid() const { return isType(TIME_CONFIG); }
    /// @brief Converts a byte buffer in-place to this message type, without any runtime checking.
    /// @param data The byte buffer to interpret a message from.
    /// @return The resulting message object.
    constexpr static Message<TIME_CONFIG>& fromData(uint8_t* data) { return *reinterpret_cast<Message<TIME_CONFIG>*>(data); }
} __attribute__((packed));

template<>
class Message<SENSOR_DATA> : public MessageHeader
{
private:
    /// @brief The timestamp associated with the held values (UNIX epoch, seconds).
    uint32_t time;
    /// @brief The amount of values held in the messages' values array.
    uint8_t nValues;

public:
    /// @brief The maximum amount of sensor values that can be held in a single sensor data message.
    static const size_t maxNValues = (maxLength - headerLength - sizeof(time) - sizeof(nValues)) / sizeof(SensorValue);

private:
    std::array<SensorValue, maxNValues> values{};

public:
    Message(const MACAddress& src, const MACAddress& dest, uint32_t time, const uint8_t nValues, const std::array<SensorValue, maxNValues> values)
      : MessageHeader(SENSOR_DATA, src, dest)
      , time{ time }
      , nValues{ std::min(nValues, static_cast<uint8_t>(maxNValues)) }
      , values{ values } {};

    uint32_t getCTime() const { return time; };
    uint32_t getNValues() const { return nValues; };
    std::array<SensorValue, maxNValues>& getValues() { return values; }

    /// @return The messages' length in bytes.
    constexpr size_t getLength() const { return headerLength + sizeof(time) + sizeof(nValues) + nValues * sizeof(SensorValue); };
    /// @return Whether the message's type flag matches the desired type.
    constexpr bool isValid() const { return isType(SENSOR_DATA); }
    /// @brief Converts a byte buffer in-place to this message type, without any runtime checking.
    /// @param data The byte buffer to interpret a message from.
    /// @return The resulting message object.
    static constexpr Message<SENSOR_DATA>& fromData(uint8_t* data);
} __attribute__((packed));

constexpr Message<SENSOR_DATA>&
Message<SENSOR_DATA>::fromData(uint8_t* data)
{
    Message<SENSOR_DATA>& m{ *reinterpret_cast<Message<SENSOR_DATA>*>(data) };
    m.nValues = std::min(m.nValues, static_cast<uint8_t>(maxNValues));
    return m;
}

#endif
