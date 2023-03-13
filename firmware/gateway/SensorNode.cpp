#include "SensorNode.h"
#include <cstddef>
#include "utilities.h"
#include <PCF2129_RTC.h>

extern PCF2129_RTC rtc;


void SensorNodeInit(SensorNode_t* node, uint8_t macAddress[6]){
    size_t i;

    for(i = 0; i < 6; i++){
        node->macAddresss[i] = macAddress[i];
    }
}

void SensorNodeSetMACAddress(SensorNode_t* node, uint8_t* address){
    size_t i;
    for(i = 0; i < ARRAY_LENGTH(node->macAddresss); i++){
        node->macAddresss[i] = address[i];
    }
}

void SensorNodeUpdateTimes(SensorNode_t* node){
    uint32_t time = rtc.read_time_epoch();
    while(node->nextCommunicationTime <= time){
        node->nextCommunicationTime += communicationInterval;
    }
}

void SensorNodeLogError(SensorNode_t* node){
    // TODO
}
