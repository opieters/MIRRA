#include "CommunicationCommon.h"

uint8_t CommunicationCommandToInt(CommunicationCommand cmd){
    switch(cmd){
        case CommunicationCommand::TIME_CONFIG:
            return 1;
        case CommunicationCommand::MEASUREMENT_DATA:
            return 2;
        case CommunicationCommand::ACK_DATA:
            return 3;
        case CommunicationCommand::HELLO:
            return 4;
        case CommunicationCommand::REQUEST_MEASUREMENT_DATA:
            return 5;
        case CommunicationCommand::HELLO_REPLY:
            return 6;
        case CommunicationCommand::NONE:
            return 0;
    }
    return 0;
}

CommunicationCommand IntToCommunicationCommand(uint8_t cmd){
    switch(cmd){
        case 1:
            return CommunicationCommand::TIME_CONFIG;
        case 2:
            return CommunicationCommand::MEASUREMENT_DATA;
        case 3:
            return CommunicationCommand::ACK_DATA;
        case 4: 
            return CommunicationCommand::HELLO;
        case 5:
            return CommunicationCommand::REQUEST_MEASUREMENT_DATA;
        case 6:
            return CommunicationCommand::HELLO_REPLY;
        default:
            return CommunicationCommand::NONE;
    }
    return CommunicationCommand::NONE;
}

bool arrayCompare(void* array1, void* array2, size_t length){
    uint8_t *a1 = (uint8_t*) array1, *a2 = (uint8_t*) array2;
    size_t i = 0;
    while(i < length){
        if(a1[i] != a2[i]){
            return false;
        }
        i++;
    }
    return true;
}
