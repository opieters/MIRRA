#include "CommunicationCommon.h"
size_t Message::to_data(uint8_t *data)
{
    size_t written = 0;
    data[written] = (uint8_t)this->type;
    written += sizeof(this->type);
    memcpy(&data[written], this->src.getAddress(), MACAddress::length);
    written += MACAddress::length;
    memcpy(&data[written], this->src.getAddress(), MACAddress::length);
    written += MACAddress::length;
    return written;
}
Message Message::from_data(uint8_t *data)
{
    Message::Type type = (Message::Type)data[0];
    MACAddress src = MACAddress(&data[sizeof(Message::Type)]);
    MACAddress dest = MACAddress(&data[sizeof(Message::Type) + MACAddress::length]);
    switch (type)
    {
    case Message::Type::TIME_CONFIG:
        return TimeConfigMessage(src, dest, (uint32_t *)(&data[sizeof(Message::Type) + 2 * MACAddress::length]));
    case Message::Type::MEASUREMENT_DATA:
        break;
    default:
        return Message(type, src, dest);
    }
}
TimeConfigMessage::TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t cur_time, uint32_t sample_time, uint32_t sample_period, uint32_t comm_time, uint32_t comm_period) : Message(Message::Type::TIME_CONFIG, src, dest)
{
    this->cur_time = cur_time;
    this->sample_time = sample_time;
    this->sample_period = sample_period;
    this->comm_time = comm_time;
    this->comm_period = comm_period;
}

TimeConfigMessage::TimeConfigMessage(MACAddress src, MACAddress dest, uint32_t *data) : Message(Message::Type::TIME_CONFIG, src, dest)
{
    this->cur_time = data[0];
    this->sample_time = data[1];
    this->sample_period = data[2];
    this->comm_time = data[3];
    this->comm_period = data[4];
}

size_t TimeConfigMessage::to_data(uint8_t *data)
{
    size_t written = Message::to_data(data);
    uint32_t block[] = {cur_time, sample_time, sample_period, comm_time, comm_period};
    memcpy(&data[written], block, sizeof(cur_time));
    written += sizeof(block);
    return written;
}

MACAddress::MACAddress(const uint8_t *address)
{
    memcpy(this->address, address, MACAddress::length);
}

char *MACAddress::toString(char *string)
{
    if (strlen(string) < 17)
        return string;
    for (size_t i = 0; i < MACAddress::length; i++)
    {
        if (i != 0)
            string[3 * i - 1] = ':';
        snprintf(&string[3 * i], 2, "%02X", this->address[i]);
    }
    for (size_t i = 2; i < 17; i += 3)
    {
        string[i] = ':';
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

uint8_t zeros[] = {0, 0, 0, 0, 0, 0};
const MACAddress MACAddress::broadcast = MACAddress(zeros);
