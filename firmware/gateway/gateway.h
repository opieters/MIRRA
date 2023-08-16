#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "Commands.h"
#include "MIRRAModule.h"
#include "PubSubClient.h"
#include "WiFi.h"
#include "config.h"
#include <vector>

#define COMM_PERIOD_LENGTH(MAX_MESSAGES) ((MAX_MESSAGES * SENSOR_DATA_TIMEOUT + TIME_CONFIG_TIMEOUT) / 1000)
#define IDEAL_MESSAGES(COMM_INTERVAL, SAMP_INTERVAL) (COMM_INTERVAL / SAMP_INTERVAL)
#define MAX_MESSAGES(COMM_INTERVAL, SAMP_INTERVAL) ((3 * COMM_INTERVAL / (2 * SAMP_INTERVAL)) + 1)

/// @brief Representation of a Sensor Node's attributes relevant for communication, used for tracking the status of nodes from the gateway.
class Node
{
private:
    MACAddress mac{};
    uint32_t sampleInterval{0};
    uint32_t sampleRounding{0};
    uint32_t sampleOffset{0};
    uint32_t lastCommTime{0};
    uint32_t commInterval{0};
    uint32_t nextCommTime{0};
    uint32_t maxMessages{0};
    uint32_t errors{0};

public:
    Node() {}
    Node(Message<TIME_CONFIG>& m) : mac{m.getDest()} { timeConfig(m); }
    /// @brief Configures the Node with a time config message, the same way the actual module would do.
    /// @param m Time Config message used to saturate the representation's attributes.
    void timeConfig(Message<TIME_CONFIG>& m);
    /// @brief Configures the Node as if the time config message was missed, the same way the actual module would do.
    void naiveTimeConfig(uint32_t cTime);

    const MACAddress& getMACAddress() const { return mac; }
    uint32_t getSampleInterval() const { return sampleInterval; }
    uint32_t getSampleRounding() const { return sampleRounding; }
    uint32_t getSampleOffset() const { return sampleOffset; }
    uint32_t getLastCommTime() const { return lastCommTime; }
    uint32_t getCommInterval() const { return commInterval; }
    uint32_t getNextCommTime() const { return nextCommTime; }
    uint32_t getMaxMessages() const { return maxMessages; }

    void setSampleInterval(uint32_t sampleInterval) { this->sampleInterval = sampleInterval; }
    void setSampleRounding(uint32_t sampleRounding) { this->sampleRounding = sampleRounding; }
    void setSampleOffset(uint32_t sampleOffset) { this->sampleOffset = sampleOffset; }
};

class Gateway : public MIRRAModule
{
public:
    Gateway(const MIRRAPins& pins);
    void wake();

    class Commands : public BaseCommands<Gateway>
    {
        CommandCode changeWifi();
        void discoveryLoop(char* arg);
        void printSchedule();

    public:
        CommandCode processCommands(char* command);
    };

private:
    WiFiClient mqttClient;
    PubSubClient mqtt;

    std::vector<Node> nodes;
    std::optional<std::reference_wrapper<Node>> macToNode(char* mac);

    void discovery();

    void nodesFromFile();
    void updateNodesFile();

    void commPeriod();
    uint32_t nextScheduledCommTime();
    bool nodeCommPeriod(Node& n, std::vector<Message<SENSOR_DATA>>& data);

    const size_t topic_size = sizeof(TOPIC_PREFIX) + 1 + MACAddress::stringLength + MACAddress::stringLength;
    bool mqttConnect();
    char* createTopic(char* topic, const MACAddress& nodeMAC);
    void uploadPeriod();
    void parseUpdate(char* updateString);

    void wifiConnect(const char* SSID, const char* password);
    void wifiConnect();
    void rtcUpdateTime();
};

#endif
