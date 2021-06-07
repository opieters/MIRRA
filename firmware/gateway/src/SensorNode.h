#ifndef __SENSOR_NODE_H__
#define __SENSOR_NODE_H__

#include <stdint.h>
#include <Arduino.h>

//#define DEBUG_TIMING


typedef struct SensorNode_s {
    uint8_t macAddresss[6];
    uint32_t nextCommunicationTime;
    uint32_t lastCommunicationTime;
} SensorNode_t;

#ifdef DEBUG_TIMING
static constexpr uint32_t sampleInterval = 4*60;        // seconds
static constexpr uint32_t communicationInterval = 8*60; // seconds
#else
static constexpr uint32_t sampleInterval = 20*60;        // seconds
static constexpr uint32_t communicationInterval = 60*60; // seconds
#endif

void SensorNodeInit(SensorNode_t* node, uint8_t macAddress[6]);
void SensorNodeSetMACAddress(SensorNode_t* node, uint8_t* address);
void SensorNodeUpdateTimes(SensorNode_t* node);
void SensorNodeLogError(SensorNode_t* node);

#endif