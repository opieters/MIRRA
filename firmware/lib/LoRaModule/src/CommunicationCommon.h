#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "Sensor.h"

// TODO : move from internal structs to serializing directly from/to the class object

class MACAddress
{
private:
    uint8_t address[6]{};

public:
    MACAddress() = default;
    MACAddress(const uint8_t *address);
    uint8_t *getAddress() { return address; };
    char *toString(char *string = str_buffer) const;
    friend bool operator==(const MACAddress &mac1, const MACAddress &mac2);
    friend bool operator!=(const MACAddress &mac1, const MACAddress &mac2) { return !(operator==(mac1, mac2)); };

    static const size_t length = sizeof(address);
    static const size_t string_length = length * 3;
    static const MACAddress broadcast;
    static char str_buffer[string_length];
} __attribute__((packed));

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

    struct HeaderStruct
    {
        Message::Type type;
        MACAddress src, dest;
    } __attribute__((packed));

protected:
    HeaderStruct header{};

public:
    Message() = default;
    Message(HeaderStruct &header) : header{header} {};
    Message(Message::Type type, MACAddress src, MACAddress dest) : header{type, src, dest} {};
    Message(uint8_t *data) : Message(*reinterpret_cast<HeaderStruct *>(data)){};
    Message::Type getType() const { return header.type; };
    MACAddress getSource() const { return header.src; };
    MACAddress getDest() const { return header.dest; };
    void setDest(MACAddress dest) { header.dest = dest; }

    bool isType(Message::Type type) const { return header.type == type; };

    size_t getLength() const { return header_length; };
    uint8_t *to_data(uint8_t *data) const;
    static Message from_data(uint8_t *data);

    static const size_t header_length = sizeof(HeaderStruct);
    static const size_t max_length = 256;
};

class TimeConfigMessage : public Message
{
public:
    struct TimeConfigStruct
    {
        uint32_t cur_time, sample_time, sample_interval, comm_time, comm_interval, comm_duration;
    } __attribute__((packed));

private:
    TimeConfigStruct body{};

public:
    TimeConfigMessage() = default;
    TimeConfigMessage(HeaderStruct header, TimeConfigStruct body) : Message(header), body{body} {};
    TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t cur_time, uint32_t sample_time, uint32_t sample_interval, uint32_t comm_time, uint32_t comm_interval, uint32_t comm_duration) : Message(Message::TIME_CONFIG, src, dest), body{cur_time, sample_time, sample_interval, comm_time, comm_interval, comm_duration} {};
    TimeConfigMessage(uint8_t *data) : Message(data), body{*reinterpret_cast<TimeConfigStruct *>(&data[header_length])} {};
    uint32_t getCTime() { return body.cur_time; };
    uint32_t getSampleTime() { return body.sample_time; };
    uint32_t getSampleInterval() { return body.sample_interval; };
    uint32_t getCommTime() { return body.comm_time; };
    uint32_t getCommInterval() { return body.comm_interval; };
    uint32_t getCommDuration() { return body.comm_duration; };
    size_t getLength() const { return Message::getLength() + sizeof(body); };
    uint8_t *to_data(uint8_t *data) const;
};

class SensorDataMessage : public Message
{
public:
    struct SensorDataStruct
    {
        uint32_t time;
        uint8_t n_values;
    } __attribute__((packed));

private:
    SensorDataStruct body{};

public:
    static const size_t max_n_values = (max_length - header_length - sizeof(body)) / sizeof(SensorValue);

private:
    SensorValue values[max_n_values];

public:
    SensorDataMessage() = default;
    SensorDataMessage(HeaderStruct header, SensorDataStruct body) : Message(header), body{body} {};
    SensorDataMessage(MACAddress src, MACAddress dest, uint32_t time, uint8_t n_values, SensorValue *sensor_values);
    SensorDataMessage(uint8_t *data);
    bool isLast() { return isType(Message::SENSOR_DATA_LAST); }
    void setLast() { header.type = Message::SENSOR_DATA_LAST; }
    size_t getLength() const { return Message::getLength() + sizeof(body) + body.n_values * sizeof(SensorValue); };
    uint8_t *to_data(uint8_t *data) const;
};

#endif
