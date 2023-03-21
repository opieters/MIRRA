#include "CommunicationCommon.h"

char *MACAddress::to_string(char *string)
{
    for (size_t i = 0; i < 6; i++)
    {
        snprintf(string[2 * i], 2, "%02X", this->address[i]);
    }
}

bool operator==(const MACAddress &mac1, const MACAddress &mac2)
{
    size_t i = 0;
    for (size_t i = 0; i < MACAddress->length; i++)
    {
        if (mac1.address[i] != mac2.address[i])
            return false;
    }
    return true;
}
