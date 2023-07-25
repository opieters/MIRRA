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
    snprintf(string, MACAddress::stringLength, "%02X:%02X:%02X:%02X:%02X:%02X", this->address[0], this->address[1], this->address[2], this->address[3],
             this->address[4], this->address[5]);
    return string;
}

MACAddress MACAddress::fromString(char* string)
{
    uint8_t address[6];
    sscanf(string, "%02X:%02X:%02X:%02X:%02X:%02X", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5]);
    return MACAddress(address);
}
const MACAddress MACAddress::broadcast{};
char MACAddress::strBuffer[MACAddress::stringLength];
