#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "MIRRAModule.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include "config.h"
#include <vector>

class Node
{
private:
        MACAddress mac = MACAddress();
        uint32_t last_comm_time = 0;
        uint32_t next_comm_time = 0;
        uint32_t comm_interval = 0;
        uint32_t next_sample_time = 0;
        uint32_t sample_interval = 0;
        uint8_t errors = 0;

public:
        Node(){};
        Node(MACAddress mac, uint32_t last_comm_time, uint32_t next_comm_time) : mac{mac}, last_comm_time{last_comm_time}, next_comm_time{next_comm_time} {};
        MACAddress getMACAddress() { return mac; }
        void updateCommTime(uint32_t last_comm_time, uint32_t next_comm_time);
        void updateSampleTime(uint32_t next_sample_time, uint32_t sample_interval = SAMPLING_INTERVAL);
        uint32_t getLastCommTime() { return last_comm_time; }
        uint32_t getNextCommTime() { return next_comm_time; }
        uint32_t getNextSampleTime() { return next_sample_time; }
};

class Gateway : public MIRRAModule
{
public:
        Gateway(const MIRRAPins &pins);

        void wake();

        CommandCode processCommands(char *command);

        void discovery();

        void storeNode(Node &n);
        void nodesFromFile();
        void updateNodesFile();
        void printNodes();

        void commPeriod();
        void nodeCommPeriod(Node &n, File &dataFile);

        void storeSensorData(SensorDataMessage &m, File &dataFile);
        void pruneSensorData(File &dataFile);
        void printSensorData();

        char *createTopic(char *buffer, size_t buffer_size, MACAddress &nodeMAC);
        void uploadSensorData(File &dataFile);

        void wifiConnect(const char *SSID, const char *password);
        void wifiConnect();
        uint32_t getWiFiTime();
        uint32_t getGSMTime();
        void rtcUpdateTime();

private:
        WiFiClient mqtt_client;
        PubSubClient mqtt;

        std::vector<Node> nodes;
};

#endif
