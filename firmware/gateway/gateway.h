#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "MIRRAModule.h"
#include "WiFi.h"
#include "config.h"
#include <NTPClient.h>
#include <PubSubClient.h>
#include <vector>

class Node
{
private:
    MACAddress mac{};
    uint32_t lastCommTime{0};
    uint32_t nextSampleTime{0};
    uint32_t sampleInterval{0};
    uint32_t nextCommTime{0};
    uint32_t commInterval{0};
    uint32_t commDuration{0};
    uint8_t errors{0};

public:
    Node(){};
    Node(Message<TIME_CONFIG>& m) : mac{m.getDest()} { timeConfig(m); };
    void timeConfig(Message<TIME_CONFIG>& m);
    void naiveTimeConfig();
    const MACAddress& getMACAddress() const { return mac; }
    uint32_t getLastCommTime() const { return lastCommTime; }
    uint32_t getNextCommTime() const { return nextCommTime; }
    uint32_t getNextSampleTime() const { return nextSampleTime; }
};

class Gateway : public MIRRAModule
{
public:
    Gateway(const MIRRAPins& pins);
    void wake();
    class Commands : public MIRRAModule::Commands<Gateway>
    {
    public:
        Commands(Gateway* parent) : MIRRAModule::Commands<Gateway>(parent){};
        CommandCode processCommands(char* command);
    };

private:
    void discovery();

    void nodesFromFile();
    void updateNodesFile();
    void printNodes();

    void commPeriod();
    bool nodeCommPeriod(Node& n, std::vector<Message<SENSOR_DATA>>& data);

    const size_t topic_size = sizeof(TOPIC_PREFIX) + 1 + MACAddress::string_length + MACAddress::string_length;
    bool mqttConnect();
    char* createTopic(char* topic, const MACAddress& nodeMAC);
    void uploadPeriod();

    void wifiConnect(const char* SSID, const char* password);
    void wifiConnect();
    uint32_t getWiFiTime();
    void rtcUpdateTime();

    WiFiClient mqttClient;
    PubSubClient mqtt;

    std::vector<Node> nodes;
};

#endif
