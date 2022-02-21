#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include "Radio.h"
#include "CommunicationCommon.h"
#include "SPIFFS.h"
#include "Sensor.h"

#define ARRAY_LENGTH(X) (sizeof(X) / sizeof(X[0]))
#define SECONDS_TO_US(X) ((X)*1000000)
 
enum class CommunicationState {
    SEARCHING_GATEWAY,
    READ_SENSOR_DATA,
    UPLOAD_SENSOR_DATA,
    SLEEP,
    ERROR,
    GET_SAMPLE_CONFIG,
    UART_READOUT,
};


size_t readoutToBuffer(uint8_t* buffer, size_t max_length, uint32_t ctime, Sensor** sensorArray, uint8_t n_sensors);
void deepSleep(float sleep_time);

bool arrayCompare(uint8_t arr1[], uint8_t arr2[], size_t size1, size_t size2);
void processAckMessage(uint8_t* buffer);

bool checkCommand(LoRaMessage& message, CommunicationCommand command);

bool readTimeData(LoRaMessage& message, Sensor** sensors, size_t n_sensors);

size_t readMeasurementData(uint8_t* buffer, const size_t max_length, File& file);

#endif