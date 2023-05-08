#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "Sensor.h"

// TODO: Use unpadded structs to easily group blocks of data to be written to a buffer in the to_data methods, should improve readability substantially

class MACAddress
{
private:
    uint8_t address[6];

public:
    MACAddress() : address{0, 0, 0, 0, 0, 0} {};
    MACAddress(const uint8_t *address);
    uint8_t *getAddress() { return address; };
    char *toString(char *string = str_buffer);
    friend bool operator==(const MACAddress &mac1, const MACAddress &mac2);
    friend bool operator!=(const MACAddress &mac1, const MACAddress &mac2) { return !(operator==(mac1, mac2)); };

    static const size_t length = sizeof(address);
    static const size_t string_length = length * 3;
    static const MACAddress broadcast;
    static char str_buffer[string_length];
};

class Message
{
public:
    enum Type : uint8_t
    {
        ERROR = 0,
        HELLO = 1,
        HELLO_REPLY = 2,
        TIME_CONFIG = 3,
        ACK_TIME = 4,
        SENSOR_DATA = 5,
        SENSOR_DATA_LAST = 6,
        DATA_ACK = 7,
        REPEAT = 8,
        ALL = 9
    };

private:
    Message::Type type;
    MACAddress src, dest;

public:
    Message(Message::Type type, MACAddress src, MACAddress dest) : type{type}, src{src}, dest{dest} {};
    Message::Type getType() { return type; };
    MACAddress getSource() { return src; };
    MACAddress getDest() { return dest; };

    bool isType(Message::Type type) { return type == type; };

    virtual size_t getLength() { return header_length; };
    virtual uint8_t *to_data(uint8_t *data);
    static Message from_data(uint8_t *data);

    static const Message error;
    static const size_t header_length = sizeof(type) + 2 * MACAddress::length;
    static const size_t max_length = 256;
};

class TimeConfigMessage : public Message
{
private:
    uint32_t cur_time, sample_time, sample_interval, comm_time, comm_interval;

public:
    TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t cur_time, uint32_t sample_time, uint32_t sample_interval, uint32_t comm_time, uint32_t comm_interval);
    TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t *data);
    uint32_t getCTime() { return cur_time; };
    uint32_t getSampleTime() { return sample_time; };
    uint32_t getSampleInterval() { return sample_interval; };
    uint32_t getCommTime() { return comm_time; };
    uint32_t getCommInterval() { return comm_interval; };
    size_t getLength();
    uint8_t *to_data(uint8_t *data);
};

class SensorDataMessage : public Message
{
private:
    uint32_t time;
    uint8_t n_values;

public:
    static const size_t max_n_values = (max_length - header_length - sizeof(time) - sizeof(n_values)) / SensorValue::length;

private:
    SensorValue sensor_values[max_n_values];

public:
    SensorDataMessage(MACAddress src, MACAddress dest, uint32_t time, uint8_t n_values, SensorValue *sensor_values);
    SensorDataMessage(MACAddress src, MACAddress dest, uint8_t *data);
    bool isLast() { return isType(Message::SENSOR_DATA_LAST); }
    size_t getLength();
    uint8_t *to_data(uint8_t *data);
};

#endif
