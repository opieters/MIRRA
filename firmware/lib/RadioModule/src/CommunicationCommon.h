#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>

enum CommunicationCommand
{
    HELLO_REPLY = 6,
    TIME_CONFIG = 1,
    MEASUREMENT_DATA = 2,
    ACK_DATA = 3,
    REQUEST_MEASUREMENT_DATA = 5,
    HELLO = 4,
    NONE = 0
};

uint8_t CommunicationCommandToInt(CommunicationCommand cmd);

CommunicationCommand IntToCommunicationCommand(uint8_t cmd);

bool arrayCompare(void *array1, void *array2, size_t length);

#endif
