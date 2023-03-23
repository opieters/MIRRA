#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

class Message
{
public:
    enum Type : uint8_t
    {
        NONE = 0,
        HELLO = 1,
        HELLO_REPLY = 2,
        TIME_CONFIG = 3,
        MEASUREMENT_DATA = 4,
        ACK_DATA = 5
    };

private:
    Message::Type type;
    MACAddress src;
    MACAddress dest;

public:
    Message(Message::Type type, MACAddress src, MACAddress dest) : type{type}, src{src}, dest{dest} {};
    Message::Type getType() { return type; };
    MACAddress getSource() { return src; };
    MACAddress getDest() { return dest; };

    bool isType(Message::Type type) { return type == type; };

    virtual size_t to_data(uint8_t *data);
    static Message from_data(uint8_t *data);

    static const size_t max_length = 256;
};

class TimeConfigMessage : public Message
{
private:
    uint32_t cur_time;
    uint32_t sample_time;
    uint32_t sample_period;
    uint32_t comm_time;
    uint32_t comm_period;

public:
    TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t cur_time, uint32_t sample_time, uint32_t sample_period, uint32_t comm_time, uint32_t comm_period);
    TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t *data);
    size_t to_data(uint8_t *data);
};

class MACAddress
{
private:
    uint8_t address[6];

public:
    MACAddress(const uint8_t *address);
    uint8_t *getAddress() { return address; };
    char *toString(char *string);
    friend bool operator==(const MACAddress &mac1, const MACAddress &mac2);
    friend bool operator!=(const MACAddress &mac1, const MACAddress &mac2) { return !(operator==(mac1, mac2)); };

    static const size_t length = sizeof(address);
    static const MACAddress broadcast;
};

#endif
