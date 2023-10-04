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

    struct Commands : CommonCommands
    {
        Gateway* parent;
        Commands(Gateway* parent) : parent{parent} {};

        /// @brief Prompts new credentials for the WiFi network to connect to.
        CommandCode changeWifi();
        /// @brief Attempts to configure the local RTC to the correct time using NTP.
        CommandCode rtcUpdateTime();
        /// @brief Forces a discovery period.
        CommandCode discovery();
        /// @brief Sends a discovery message every 2.5 minutes for the specified amount of loops.
        /// @arg The amount of times to loop.
        CommandCode discoveryLoop(char* arg);
        /// @brief Prints scheduling information about the connected nodes, including MAC address, next comm time, sample interval and max number of messages
        /// per comm period.
        CommandCode printSchedule();

        static constexpr auto getCommands()
        {
            return std::tuple_cat(CommonCommands::getCommands(),
                                  std::make_tuple(CommandAliasesPair(&Commands::changeWifi, "wifi"), CommandAliasesPair(&Commands::rtcUpdateTime, "rtc"),
                                                  CommandAliasesPair(&Commands::discovery, "discovery"),
                                                  CommandAliasesPair(&Commands::discoveryLoop, "discoveryloop"),
                                                  CommandAliasesPair(&Commands::printSchedule, "printschedule")));
        }
    };

private:
    WiFiClient mqttClient;
    PubSubClient mqtt;

    std::vector<Node> nodes;
    /// @brief Returns the local node corresponding to the MAC address.
    /// @param mac The MAC address string in the "00:00:00:00:00:00" format.
    /// @return A reference to the matching node. Disenaged if no match is found.
    std::optional<std::reference_wrapper<Node>> macToNode(char* mac);

    /// @brief Sends a single discovery message, storing the new node and configuring its timings if there is a response.
    void discovery();

    /// @brief Imports the nodes stored on the local filesystem. Used to retain the Nodes objects through deep sleep.
    void nodesFromFile();
    /// @brief Updates the nodes stored on the local filesystem. Used to retain the Nodes objects through deep sleep.
    void updateNodesFile();

    /// @brief Initiates a gateway-wide communication period.
    void commPeriod();
    /// @brief Retrieves, if possible, the next scheduled start time for a node communication period.
    /// @return The next scheduled comm period if it exists, else -1.
    uint32_t nextScheduledCommTime();
    /// @brief Initiates a comm period with a node, retrieving its sensor data and updating its timings.
    /// @param n The node to communicate with.
    /// @param data Vector to store the data in.
    /// @return Whether the communication period was successful or not.
    bool nodeCommPeriod(Node& n, std::vector<Message<SENSOR_DATA>>& data);

    /// @brief Attempts to connect to the designated MQTT server.
    /// @return Whether the connection was successful or not.
    bool mqttConnect();
    const size_t topicSize = sizeof(TOPIC_PREFIX) + MACAddress::stringLength + MACAddress::stringLength;
    /// @brief Creates a topic string with the following layout: "(TOPIC_PREFIX)/(MAC address gateway)/(MAC address node)"
    /// @param topic The topic string.
    /// @param nodeMAC The associated node's MAC address.
    /// @return The topic string.
    char* createTopic(char* topic, const MACAddress& nodeMAC);
    /// @brief Uploads stored sensor data messages to the MQTT server, and marks uploaded messages as such in the filesystem by setting the 'upload' flag to 1.
    void uploadPeriod();
    /// (UNUSED)
    /// @brief Parses a node update string with the following layout: "(MAC address node)/(sample interval)/(sample rounding)/(sample offset)"
    /// @param update The node update string.
    void parseNodeUpdate(char* updateString);

    /// @brief Attempts to connect to the WiFi network with the given credentials. If successful, these credentials are stored for later connections.
    /// @param SSID The SSID of the WiFi network to connect to.
    /// @param password The password of the WiFi network to connect to.
    void wifiConnect(const char* SSID, const char* password);
    /// @brief Attempts to connect to the WiFi network with the stored credentials.
    void wifiConnect();
};

#endif
