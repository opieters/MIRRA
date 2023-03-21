#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>

enum CommunicationCommand : uint8_t
{
    NONE = 0,
    TIME_CONFIG = 1,
    MEASUREMENT_DATA = 2,
    ACK_DATA = 3,
    HELLO = 4,
    REQUEST_MEASUREMENT_DATA = 5,
    HELLO_REPLY = 6,
};

class MACAddress
{
private:
    uint8_t address[6];

public:
    static const size_t length = 6;
    MACAddress(uint8_t *address);
    char *to_string(char *string);
    friend bool operator==(const MACAddress &mac1, const MACAddress &mac2);
    friend bool operator!=(const MACAddress &mac1, const MACAddress &mac2) { return !(operator==(mac1, mac2)); };
};

#endif
