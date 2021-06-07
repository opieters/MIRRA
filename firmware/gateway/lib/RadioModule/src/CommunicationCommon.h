#ifndef __COMM_COMM_H__
#define __COMM_COMM_H__

#include <stdint.h>
#include <cstddef>

enum class CommunicationCommand {
    HELLO_REPLY,
    TIME_CONFIG,
    MEASUREMENT_DATA,
    ACK_DATA, 
    REQUEST_MEASUREMENT_DATA,
    HELLO,
    NONE
};

uint8_t CommunicationCommandToInt(CommunicationCommand cmd);

CommunicationCommand IntToCommunicationCommand(uint8_t cmd);

bool arrayCompare(void* array1, void* array2, size_t length);

#endif
