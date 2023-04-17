#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "LoRaModule.h"
#include "PCF2129_RTC.h"
#include "PubSubClient.h"
#include <CommunicationCommon.h>
#include "WiFi.h"
#include <FS.h>
#include <UplinkModule.h>
#include "config.h"
#include <logging.h>
#include <vector>

class Node
{
private:
        MACAddress mac;
        uint32_t last_comm_time;
        uint32_t next_comm_time;
        uint32_t sampling_interval;

public:
        Node(MACAddress mac, uint32_t last_comm_time, uint32_t next_comm_time) : mac{mac}, last_comm_time{last_comm_time}, next_comm_time{next_comm_time} {}
        MACAddress getMACAddress() { return mac; }
        void updateLastCommTime(uint32_t ctime) { last_comm_time = ctime; };
        void updateNextCommTime(uint32_t time) { next_comm_time = time; };
        uint32_t getLastCommTime() {return last_comm_time;}
        uint32_t getNextCommTime() {return next_comm_time;}
};

RTC_DATA_ATTR bool firstBoot = true;

class Gateway
{
public:
        Gateway(Logger *log);
        void nodesFromFile();
        
        void deepSleep(float time);
        void deepSleepUntil(uint32_t time);

        void discovery();
        void storeNode(Node& n);

        void commPeriod();
        void storeSensorData(SensorDataMessage& m, File& dataFile);


        char *createTopic(char *buffer, size_t buffer_size, MACAddress& nodeMAC);
        void uploadPeriod();

        bool uploadData();
        bool printDataUART();

        uint32_t getWiFiTime(void);
        uint32_t getGSMTime(void);

private:
        LoRaModule lora;
        PCF2129_RTC rtc;
        PubSubClient mqtt;
        WiFiClient wifi;

        Logger *log;

        std::vector<Node> nodes;

        static const uint32_t communicationSpacing = 15; // seconds
};

#endif