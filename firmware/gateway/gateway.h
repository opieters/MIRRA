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
        uint32_t sampling_interval = 0;

public:
        Node(){};
        Node(MACAddress mac, uint32_t last_comm_time, uint32_t next_comm_time) : mac{mac}, last_comm_time{last_comm_time}, next_comm_time{next_comm_time} {};
        MACAddress getMACAddress() { return mac; }
        void updateLastCommTime(uint32_t ctime) { last_comm_time = ctime; };
        void updateNextCommTime(uint32_t time) { next_comm_time = time; };
        uint32_t getLastCommTime() { return last_comm_time; }
        uint32_t getNextCommTime() { return next_comm_time; }
};

class Gateway : public MIRRAModule
{
public:
        Gateway(const MIRRAPins &pins);
        void nodesFromFile();

        void wake();

        CommandCode processCommands(char *command);

        void discovery();
        void storeNode(Node &n);

        void commPeriod();
        void storeSensorData(SensorDataMessage &m, File &dataFile);
        void pruneSensorData(File &dataFile);

        void printSensorData();
        void printNodes();

        char *createTopic(char *buffer, size_t buffer_size, MACAddress &nodeMAC);
        void uploadPeriod();

        void wifiConnect();
        uint32_t getWiFiTime();
        uint32_t getGSMTime();

private:
        WiFiClient mqtt_client;
        PubSubClient mqtt;

        std::vector<Node> nodes;
};

#endif
