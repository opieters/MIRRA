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
        uint32_t next_sample_time = 0;
        uint32_t sample_interval = 0;
        uint32_t next_comm_time = 0;
        uint32_t comm_interval = 0;
        uint32_t comm_duration = 0;
        uint8_t errors = 0;

public:
        Node(){};
        Node(TimeConfigMessage &m) : mac{m.getDest()} { timeConfig(m); };
        void timeConfig(TimeConfigMessage &m);
        void naiveTimeConfig();
        MACAddress getMACAddress() { return mac; }
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

        void nodesFromFile();
        void updateNodesFile();
        void printNodes();

        void commPeriod();
        bool nodeCommPeriod(Node &n, File &dataFile);

        void pruneSensorData(File &dataFile);

        const size_t topic_size = sizeof(TOPIC_PREFIX) + 1 + MACAddress::string_length + MACAddress::string_length;
        bool mqttConnect();
        char *createTopic(char *topic, MACAddress const &nodeMAC);
        void uploadPeriod();

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
