#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include "Sensor.h"
#include <array>

class MACAddress
{
private:
    std::array<uint8_t, 6> address{0};

public:
    MACAddress() = default;
    MACAddress(const uint8_t* address) : address{*reinterpret_cast<const std::array<uint8_t, 6>*>(address)} {};
    uint8_t* getAddress() { return address.data(); };
    char* toString(char* string = strBuffer) const;
    bool operator==(const MACAddress& other) const { return this->address == other.address; }
    bool operator!=(const MACAddress& other) const { return this->address != other.address; }

    static MACAddress fromString(char* string);

    static const size_t length = sizeof(address);
    static const size_t stringLength = length * 3;
    static const MACAddress broadcast;
    static char strBuffer[stringLength];
} __attribute__((packed));

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

class MessageHeader
{
private:
    MessageType type{MessageType::ERROR};
    MACAddress src{}, dest{};

protected:
    MessageHeader(MessageType type, const MACAddress& src, const MACAddress& dest) : type{static_cast<MessageType>(type << 1)}, src{src}, dest{dest} {};

public:
    constexpr MessageType getType() const { return static_cast<MessageType>(type >> 1); };
    constexpr bool isType(MessageType type) const { return getType() == type; };
    constexpr bool isLast() const { return type & 0x01; }
    const MACAddress& getSource() const { return src; };
    const MACAddress& getDest() const { return dest; };
    constexpr size_t getLength() const { return sizeof(*this); };

    void setType(MessageType type) { this->type = static_cast<MessageType>(type << 1); }
    void setDest(const MACAddress& dest) { this->dest = dest; }
    void setLast() { type = static_cast<MessageType>(type | 0x01); }

    static const size_t headerLength{sizeof(MessageType) + 2 * sizeof(MACAddress)};
    static const size_t maxLength{256};
} __attribute__((packed));

template <MessageType T> class Message : public MessageHeader
{
public:
    Message(const MACAddress& src, const MACAddress& dest) : MessageHeader(T, src, dest){};
    constexpr bool isValid() const { return isType(T); }
    constexpr static Message<T>& fromData(uint8_t* data) { return *reinterpret_cast<Message<T>*>(data); }
    uint8_t* toData() { return reinterpret_cast<uint8_t*>(this); }
    static const MessageType desiredType{T};
} __attribute__((packed));

template <> class Message<TIME_CONFIG> : public MessageHeader
{
private:
    uint32_t curTime, sampleInterval, sampleRounding, sampleOffset, commInterval, commTime, maxMessages;

public:
    Message(const MACAddress& src, const MACAddress& dest, uint32_t curTime, uint32_t sampleInterval, uint32_t sampleRounding, uint32_t sampleOffset,
            uint32_t commInterval, uint32_t commTime, uint32_t maxMessages)
        : MessageHeader(TIME_CONFIG, src, dest), curTime{curTime}, sampleInterval{sampleInterval}, sampleRounding{sampleRounding}, sampleOffset{sampleOffset},
          commInterval{commInterval}, commTime{commTime}, maxMessages{maxMessages} {};

    uint32_t getCTime() const { return curTime; };
    uint32_t getSampleInterval() const { return sampleInterval; };
    uint32_t getSampleRounding() const { return sampleRounding; };
    uint32_t getSampleOffset() const { return sampleOffset; };
    uint32_t getCommInterval() const { return commInterval; };
    uint32_t getCommTime() const { return commTime; };
    uint32_t getMaxMessages() const { return maxMessages; };

    constexpr size_t getLength() const { return sizeof(*this); };
    constexpr bool isValid() const { return isType(TIME_CONFIG); }
    constexpr static Message<TIME_CONFIG>& fromData(uint8_t* data) { return *reinterpret_cast<Message<TIME_CONFIG>*>(data); }
    uint8_t* toData() { return reinterpret_cast<uint8_t*>(this); }
    static const MessageType desiredType{TIME_CONFIG};
} __attribute__((packed));

template <> class Message<SENSOR_DATA> : public MessageHeader
{
private:
    uint32_t time;
    uint8_t nValues;

public:
    static const size_t maxNValues = (maxLength - headerLength - sizeof(time) - sizeof(nValues)) / sizeof(SensorValue);

private:
    std::array<SensorValue, maxNValues> values{};

public:
    Message(const MACAddress& src, const MACAddress& dest, uint32_t time, const uint8_t nValues, const std::array<SensorValue, maxNValues> values)
        : MessageHeader(SENSOR_DATA, src, dest), time{time}, nValues{std::min(nValues, static_cast<uint8_t>(maxNValues))}, values{values} {};

    uint32_t getCTime() const { return time; };
    uint32_t getNValues() const { return nValues; };
    std::array<SensorValue, maxNValues>& getValues() { return values; }

    constexpr size_t getLength() const { return headerLength + sizeof(time) + sizeof(nValues) + nValues * sizeof(SensorValue); };
    constexpr bool isValid() const { return isType(SENSOR_DATA); }
    static constexpr Message<SENSOR_DATA>& fromData(uint8_t* data);
    uint8_t* toData() { return reinterpret_cast<uint8_t*>(this); }
    static const MessageType desiredType{SENSOR_DATA};
} __attribute__((packed));

constexpr Message<SENSOR_DATA>& Message<SENSOR_DATA>::fromData(uint8_t* data)
{
    Message<SENSOR_DATA>& m{*reinterpret_cast<Message<SENSOR_DATA>*>(data)};
    m.nValues = std::min(m.nValues, static_cast<uint8_t>(maxNValues));
    return m;
}

#endif
