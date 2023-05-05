#include "CommunicationCommon.h"
uint8_t *Message::to_data(uint8_t *data)
{
    uint8_t written = 0;
    data[written] = (uint8_t)this->type;
    written += sizeof(this->type);
    memcpy(&data[written], this->src.getAddress(), MACAddress::length);
    written += MACAddress::length;
    memcpy(&data[written], this->src.getAddress(), MACAddress::length);
    return data;
}
Message Message::from_data(uint8_t *data)
{
    uint8_t read = 0;
    Message::Type type = (Message::Type)(data[0]);
    read += sizeof(Message::Type);
    MACAddress src = MACAddress(&data[read]);
    read += MACAddress::length;
    MACAddress dest = MACAddress(&data[read]);
    read += MACAddress::length;
    switch (type)
    {
    case Message::TIME_CONFIG:
        return TimeConfigMessage(src, dest, (uint32_t *)(&data[read]));
    case Message::SENSOR_DATA:
        return SensorDataMessage(src, dest, &data[read]);
    default:
        return Message(type, src, dest);
    }
}

const Message Message::error = Message(Message::ERROR, MACAddress::broadcast, MACAddress::broadcast);

TimeConfigMessage::TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t cur_time, uint32_t sample_time, uint32_t sample_interval, uint32_t comm_time, uint32_t comm_interval) : Message(Message::TIME_CONFIG, src, dest)
{
    this->cur_time = cur_time;
    this->sample_time = sample_time;
    this->sample_interval = sample_interval;
    this->comm_time = comm_time;
    this->comm_interval = comm_interval;
}

TimeConfigMessage::TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t *data) : Message(Message::TIME_CONFIG, src, dest)
{
    this->cur_time = data[0];
    this->sample_time = data[1];
    this->sample_interval = data[2];
    this->comm_time = data[3];
    this->comm_interval = data[4];
}

size_t TimeConfigMessage::getLength()
{
    return header_length + sizeof(cur_time) + sizeof(sample_time) + sizeof(sample_interval) + sizeof(comm_time) + sizeof(comm_interval);
}

uint8_t *TimeConfigMessage::to_data(uint8_t *data)
{
    data = Message::to_data(data);
    uint32_t block[] = {cur_time, sample_time, sample_interval, comm_time, comm_interval};
    memcpy(&data[Message::getLength()], block, sizeof(block));
    return data;
}

SensorDataMessage::SensorDataMessage(MACAddress src, MACAddress dest, uint32_t time, uint8_t n_values, SensorValue *sensor_values) : Message(Message::SENSOR_DATA, src, dest)
{
    this->time = time;
    this->n_values = n_values;
    for (size_t i = 0; i < n_values; i++)
    {
        this->sensor_values[i] = sensor_values[i];
    }
}

SensorDataMessage::SensorDataMessage(MACAddress src, MACAddress dest, uint8_t *data) : Message(Message::SENSOR_DATA, src, dest)
{
    size_t read = 0;
    this->time = ((uint32_t *)data)[read];
    read += sizeof(this->time);
    this->n_values = data[read];
    for (size_t i = 0; i < this->n_values; i++)
    {
        this->sensor_values[i] = SensorValue(&data[read + i * SensorValue::length]);
    }
}

size_t SensorDataMessage::getLength()
{
    return header_length + sizeof(this->time) + sizeof(n_values) + this->n_values * SensorValue::length;
}

uint8_t *SensorDataMessage::to_data(uint8_t *data)
{
    Message::to_data(data);
    size_t written = header_length;
    uint32_t *time_p = (uint32_t *)(&data[written]);
    time_p[0] = this->time;
    written += sizeof(this->time);
    float *n_values_p = (float *)(&data[written]);
    n_values_p[0] = this->n_values;
    written += sizeof(this->n_values);
    for (size_t i = 0; i < this->n_values; i++)
    {
        this->sensor_values[i].to_data(&data[written + i * SensorValue::length]);
    }
    return data;
}

MACAddress::MACAddress(const uint8_t *address)
{
    memcpy(this->address, address, MACAddress::length);
}

char *MACAddress::toString(char *string)
{
    if (strlen(string) + 1 < MACAddress::string_length)
        return string;
    for (size_t i = 0; i < MACAddress::length; i++)
    {
        if (i != 0)
            string[3 * i - 1] = ':';
        snprintf(&string[3 * i], 2, "%02X", this->address[i]);
    }
    return string;
}

bool operator==(const MACAddress &mac1, const MACAddress &mac2)
{
    size_t i = 0;
    for (size_t i = 0; i < MACAddress::length; i++)
    {
        if (mac1.address[i] != mac2.address[i])
            return false;
    }
    return true;
}

const MACAddress MACAddress::broadcast = MACAddress();
char MACAddress::str_buffer[MACAddress::string_length];
