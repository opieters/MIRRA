#include "CommunicationCommon.h"

MACAddress::MACAddress(uint8_t *address)
{
    for (size_t i = 0; i < MACAddress::length; i++)
    {
        this->address[i] = address[i];
    }
}

char *MACAddress::to_string(char *string)
{
    if (strlen(string) < 17)
        return string;
    for (size_t i = 0; i < 6; i++)
    {
        snprintf(&string[2 * i], 2, "%02X", this->address[i]);
    }
    for (size_t i = 2; i < 17; i += 3)
    {
        snprintf(&string[i], 1, ":");
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
