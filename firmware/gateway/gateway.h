#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "Radio.h"
#include "SensorNode.h"
#include "PCF2129_RTC.h"
#include "PubSubClient.h"
#include <CommunicationCommon.h>
#include "WiFi.h"
#include <FS.h>
#include <UplinkModule.h>

//#define DEBUG_TIMING

enum class GateWayState {
    UPLOAD_DATA,
    DISCOVER_MODULES,
    READ_SENSORS,
    ERROR,
    UART_READOUT,
    FIND_NEXT_STATE,
    MODULE_CHECK,
};

class Gateway {
    public:
        Gateway(RadioModule*, PCF2129_RTC*, PubSubClient*, WiFiClient* espClient, UplinkModule* module);

        bool sendMQTT();

        void deepSleep(float time);
        SensorNode_t* addNewNode(LoRaMessage& m);

        bool uploadData();
        bool printDataUART();
        void storeSensorData(LoRaMessage&);
        void createDiscoveryMessage(LoRaMessage&);
        void createAckMessage(LoRaMessage&, LoRaMessage&);
        void createUpdateMessage(SensorNode_t&, LoRaMessage&);
        void createReadoutMessage(SensorNode_t& node, LoRaMessage& message);
        bool checkNodeSource(SensorNode_t& node, LoRaMessage& message, CommunicationCommand cmd);

        void createTopic(char* buffer, size_t buffer_size, uint8_t* nodeMAC);

        void initFirstBoot(void);

        static const uint32_t getUploadDataInterval(void) { return Gateway::uploadDataInterval; };

        uint32_t getWiFiTime(void);
        uint32_t getGSMTime(void);
    private:
        RadioModule* radioModule;
        PCF2129_RTC* rtc;
        PubSubClient* mqtt;
        WiFiClient* espClient;

        UplinkModule* uplinkModule;

        char clientID[7+6*2+6];

        static const uint32_t communicationSpacing = 15; // seconds

#ifdef DEBUG_TIMING
        static const uint32_t uploadDataInterval = 60; // seconds
#else
        static const uint32_t uploadDataInterval = 60*60; // seconds
#endif
        static const char* topic;  //!< MQTT topic = `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`
};

bool openSensorDataFile(void);

File openDataFile(void);

#endif