#include "CommunicationCommon.h"
#include <algorithm>

Message<SENSOR_DATA>::Message(const MACAddress& src, const MACAddress& dest, uint32_t time, const uint8_t nValues,
                              const std::array<SensorValue, maxNValues>& values)
    : MessageHeader(SENSOR_DATA, src, dest), time{time}, nValues{std::min(nValues, static_cast<uint8_t>(maxNValues))}
{
    std::copy(values.begin(), values.begin() + this->nValues * sizeof(SensorValue), this->values.begin());
}

char* MACAddress::toString(char* string) const
{
    snprintf(string, MACAddress::string_length, "%02X:%02X:%02X:%02X:%02X:%02X", this->address[0], this->address[1], this->address[2], this->address[3],
             this->address[4], this->address[5]);
    return string;
}

const MACAddress MACAddress::broadcast{};
char MACAddress::str_buffer[MACAddress::string_length];
