#include "gateway.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include <cstring>

void Node::timeConfig(Message<TIME_CONFIG>& m)
{
    this->lastCommTime = m.getCTime();
    this->nextSampleTime = (m.getSampleTime() == 0) ? nextSampleTime : m.getSampleTime();
    this->sampleInterval = (m.getSampleInterval() == 0) ? sampleInterval : m.getSampleInterval();
    this->nextCommTime = m.getCommTime();
    this->commInterval = m.getCommInterval();
    this->commDuration = m.getCommDuration();
}

void Node::naiveTimeConfig() { nextCommTime += commInterval; }

RTC_DATA_ATTR bool initialBoot{true};
RTC_DATA_ATTR int commPeriods{0};

RTC_DATA_ATTR char ssid[32]{WIFI_SSID};
RTC_DATA_ATTR char pass[32]{WIFI_PASS};

Gateway::Gateway(const MIRRAPins& pins) : MIRRAModule(pins), mqttClient{WiFiClient()}, mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, mqttClient)}
{
    if (initialBoot)
    {
        Log::info("First boot.");
        // manage filesystem
        if (LittleFS.exists(NODES_FP))
            LittleFS.remove(NODES_FP);
        updateNodesFile();
        if (LittleFS.exists(DATA_FP))
            LittleFS.remove(DATA_FP);
        File dataFile{LittleFS.open(DATA_FP, "w", true)};
        dataFile.close();

        rtcUpdateTime();
        initialBoot = false;
    }
    Log::info("Used ", LittleFS.usedBytes() / 1000, "KB of ", LittleFS.totalBytes() / 1000, "KB available on flash.");
    nodesFromFile();
}

void Gateway::wake()
{
    Log::debug("Running wake()...");
    if (!nodes.empty() && time(nullptr) >= WAKE_COMM_PERIOD(nodes[0].getNextCommTime()))
        commPeriod();
    // send data to server only every UPLOAD_EVERY comm periods
    if (commPeriods >= UPLOAD_EVERY)
    {
        uploadPeriod();
    }
    Serial.printf("Welcome! This is Gateway %s\n", lora.getMACAddress().toString());
    Commands(this).prompt();
    Log::debug("Entering deep sleep...");
    if (nodes.empty())
        deepSleep(COMMUNICATION_INTERVAL);
    else
        deepSleepUntil(WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

void Gateway::discovery()
{
    if (nodes.size() >= MAX_SENSOR_NODES)
    {
        Log::info("Could not run discovery because maximum amount of nodes has been reached.");
        return;
    }
    Log::info("Sending discovery message.");
    lora.sendMessage(Message<HELLO>(lora.getMACAddress(), MACAddress::broadcast));
    Log::debug("Awaiting discovery response message ...");
    auto helloReply{lora.receiveMessage<HELLO_REPLY>(DISCOVERY_TIMEOUT)};
    if (!helloReply)
    {
        Log::error("Error while awaiting/receiving reply to discovery message. Aborting discovery.");
        return;
    }
    Log::info("Node found at ", helloReply->getSource().toString());

    uint32_t cTime{time(nullptr)};
    uint32_t sampleTime{((cTime + SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING};
    uint32_t commTime{nodes.empty() ? cTime + COMMUNICATION_INTERVAL
                                    : nodes.back().getNextCommTime() + COMMUNICATION_PERIOD_LENGTH + COMMUNICATION_PERIOD_PADDING};

    Log::debug("Sending time config message to ", helloReply->getSource().toString());
    auto timeConfig{Message<TIME_CONFIG>(lora.getMACAddress(), helloReply->getSource(), cTime, sampleTime, SAMPLING_INTERVAL, commTime, COMMUNICATION_INTERVAL,
                                         COMMUNICATION_PERIOD_LENGTH)};
    lora.sendMessage(timeConfig);
    auto time_ack{lora.receiveMessage<ACK_TIME>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, helloReply->getSource())};
    if (!time_ack)
    {
        Log::error("Error while receiving ack to time config message from ", helloReply->getSource().toString(), ". Aborting discovery.");
        return;
    }

    Log::info("Registering node %s", time_ack->getSource().toString());
    nodes.push_back(Node(timeConfig));
    updateNodesFile();
}

void Gateway::nodesFromFile()
{
    Log::debug("Recovering nodes from file...");
    File nodesFile = LittleFS.open(NODES_FP);
    uint8_t size = nodesFile.read();
    Log::debug(size, " nodes found in ", NODES_FP);
    nodes.resize(size);
    nodesFile.read((uint8_t*)nodes.data(), size * sizeof(Node));
    nodesFile.close();
}

void Gateway::updateNodesFile()
{
    File nodesFile = LittleFS.open(NODES_FP, "w", true);
    nodesFile.write(nodes.size());
    nodesFile.write((uint8_t*)nodes.data(), nodes.size() * sizeof(Node));
    nodesFile.close();
}

void Gateway::printNodes()
{
    for (const Node& n : nodes)
    {
        Serial.printf("NODE MAC: %s, LAST COMM TIME: %u, NEXT COMM TIME: %u\n", n.getMACAddress().toString(), n.getLastCommTime(), n.getNextCommTime());
    }
}

void Gateway::commPeriod()
{
    Log::info("Starting comm period...");
    std::vector<Message<SENSOR_DATA>> data;
    data.reserve(MAX_SENSOR_MESSAGES * nodes.size());
    std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) { return a.getNextCommTime() < b.getNextCommTime(); });
    // todo : filter out nodes that are scheduled unrealistically far in the future
    for (Node& n : nodes)
    {
        if (!nodeCommPeriod(n, data))
            n.naiveTimeConfig();
    }
    if (nodes.empty())
    {
        Log::info("No comm periods performed because no nodes have been registered.");
    }
    else
    {
        updateNodesFile();
    }
    File dataFile = LittleFS.open(DATA_FP, "a");
    for (Message<SENSOR_DATA>& m : data)
    {
        storeSensorData(m, dataFile);
    }
    dataFile.close();
    commPeriods++;
}

bool Gateway::nodeCommPeriod(Node& n, std::vector<Message<SENSOR_DATA>>& data)
{
    uint32_t ctime = time(nullptr);
    if (ctime > n.getNextCommTime())
    {
        Log::error("Node ", n.getMACAddress().toString(),
                   "'s comm time was faultily scheduled before this gateway's comm period. Skipping communication with this node.");
        return false;
    }
    lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
    uint32_t listen_ms = COMMUNICATION_PERIOD_PADDING * 1000; // pre-listen in anticipation of message
    size_t messagesReceived = 0;
    while (true)
    {
        Log::debug("Awaiting data from ", n.getMACAddress().toString(), " ...");
        auto sensorData{lora.receiveMessage<SENSOR_DATA>(SENSOR_DATA_TIMEOUT, SENSOR_DATA_ATTEMPTS, n.getMACAddress(), listen_ms)};
        listen_ms = 0;
        if (!sensorData)
        {
            Log::error("Error while awaiting/receiving data from ", n.getMACAddress().toString(), ". Skipping communication with this node.");
            return false;
        }
        Log::info("Sensor data received from ", n.getMACAddress().toString(), " with length ", sensorData->getLength());
        data.push_back(*sensorData);
        messagesReceived++;
        if (sensorData->isLast() || messagesReceived >= MAX_SENSOR_MESSAGES)
        {
            Log::debug("Last message received.");
            break;
        }
        Log::debug("Sending data ACK to ", n.getMACAddress().toString(), " ...");
        lora.sendMessage(Message<ACK_DATA>(lora.getMACAddress(), n.getMACAddress()));
    }
    ctime = time(nullptr);
    uint32_t commTime = n.getNextCommTime() + COMMUNICATION_INTERVAL;
    Log::info("Sending time config message to ", n.getMACAddress().toString(), " ...");
    auto timeConfig{Message<TIME_CONFIG>(lora.getMACAddress(), n.getMACAddress(), ctime, 0, SAMPLING_INTERVAL, commTime, COMMUNICATION_INTERVAL,
                                         COMMUNICATION_PERIOD_LENGTH)};
    lora.sendMessage(timeConfig);
    auto timeAck = lora.receiveMessage<ACK_TIME>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
    if (!timeAck)
    {
        Log::error("Error while receiving ack to time config message from ", n.getMACAddress().toString(), ". Skipping communication with this node.");
        return false;
    }
    Log::info("Communication with node ", n.getMACAddress().toString(), " successful: ", messagesReceived, " messages received");
    n.timeConfig(timeConfig);
    return true;
}

void Gateway::wifiConnect(const char* SSID, const char* password)
{
    Log::info("Connecting to WiFi with SSID: ", SSID);
    WiFi.begin(SSID, password);
    for (size_t i = 10; i > 0 && WiFi.status() != WL_CONNECTED; i--)
    {
        delay(1000);
        Serial.print('.');
    }
    Serial.print('\n');
    if (WiFi.status() != WL_CONNECTED)
    {
        Log::error("Could not connect to WiFi.");
        return;
    }
    Log::info("Connected to WiFi.");
    strncpy(ssid, SSID, sizeof(ssid));
    strncpy(pass, password, sizeof(pass));
}

void Gateway::wifiConnect() { wifiConnect(ssid, pass); }

void Gateway::rtcUpdateTime()
{
    wifiConnect();
    if (WiFi.status() == WL_CONNECTED)
    {
        configTime(3600, 0, NTP_URL);
        uint64_t timeout{millis() + 10 * 1000};
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            if (millis() > timeout)
            {
                Log::error("Failed to update system time within 10s timeout");
                WiFi.disconnect();
                return;
            }
        }
        sntp_stop();
        Log::debug("Writing time to RTC...");
        rtc.write_time_epoch(static_cast<uint32_t>(time(nullptr)));
        Log::info("RTC and system time updated.");
    }
    WiFi.disconnect();
}

bool Gateway::mqttConnect()
{
    mqtt.setBufferSize(512);
    char* clientID{lora.getMACAddress().toString()};
    for (size_t i = 0; i < MQTT_ATTEMPTS; i++)
    {
        if (mqtt.connect(clientID))
            return true;
        delay(MQTT_TIMEOUT);
        if (mqtt.connected())
            return true;
    }
    return false;
}

// (TOPIC_PREFIX)/(MAC address gateway)/(MAC address node)
char* Gateway::createTopic(char* topic, const MACAddress& nodeMAC)
{
    char macStringBuffer[MACAddress::string_length];
    snprintf(topic, topic_size, "%s/%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString(), nodeMAC.toString(macStringBuffer));
    return topic;
}

/*
Upload sensor data via MQTT
data that needs to be sent is identified by a 0 (ref. storeSensorData)
sensor data is formatted as follows: MAC node (6) | MAC gateway (6) | time (4) | n_values (1) | value_data (6) | ....
value_data can be split up further in sensor_id (1) | data (4)
total length of sensor data = 17 + 6 * n_values
*/

void Gateway::uploadPeriod()
{
    Log::info("Commencing upload to MQTT server...");
    wifiConnect();
    if (WiFi.status() != WL_CONNECTED)
    {
        Log::error("WiFi not connected. Aborting upload to MQTT server...");
        return;
    }
    size_t nErrors{0}; // amount of errors while uploading
    size_t messagesPublished{0};
    bool upload{true};
    File data{LittleFS.open(DATA_FP, "r+")};
    while (data.available())
    {
        uint8_t size = data.read();
        uint8_t buffer[size];
        data.read(buffer, size);
        if (buffer[0] == 0 && upload) // upload flag: not yet uploaded
        {
            auto& message = Message<SENSOR_DATA>::fromData(buffer);
            char topic[topic_size];
            createTopic(topic, message.getSource());

            if (mqtt.connected() || mqttConnect())
            {
                if (mqtt.publish(topic, &buffer[message.headerLength], message.getLength() - message.headerLength))
                {
                    Log::debug("MQTT message succesfully published.");
                    // mark uploaded
                    size_t curPos{data.position()};
                    data.seek(curPos - size);
                    data.write(1);
                    data.seek(curPos);
                    messagesPublished++;
                }
                else
                {
                    Log::error("Error while publishing to MQTT server. State: ", mqtt.state());
                    nErrors++;
                }
            }
            else
            {
                Log::error("Error while connecting to MQTT server. Aborting upload. State: ", mqtt.state());
                upload = false;
            }
        }
        if (nErrors >= MAX_MQTT_ERRORS)
        {
            Log::error("Too many errors while publishing to MQTT server. Aborting upload.");
            upload = false;
        }
    }
    mqtt.disconnect();
    Log::info("MQTT upload finished with ", messagesPublished, " messages sent.");
    commPeriods = 0;
    data.seek(0);
    pruneSensorData(std::move(data), MAX_SENSORDATA_FILESIZE);
}

Gateway::Commands::CommandCode Gateway::Commands::processCommands(char* command)
{
    CommandCode code{MIRRAModule::Commands<Gateway>::processCommands(command)};
    if (code != CommandCode::COMMAND_NOT_FOUND)
        return code;
    if (strcmp(command, "discovery") == 0)
    {
        parent->discovery();
    }
    else if (strcmp(command, "rtc") == 0)
    {
        parent->rtcUpdateTime();
    }
    else if (strcmp(command, "wifi") == 0)
    {
        Serial.println("Enter WiFi SSID:");
        auto ssid_buffer{readLine()};
        if (!ssid_buffer)
            return COMMAND_EXIT;
        Serial.println("Enter WiFi password:");
        auto pass_buffer{readLine()};
        if (!pass_buffer)
            return COMMAND_EXIT;
        parent->wifiConnect(ssid_buffer->data(), pass_buffer->data());
        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.disconnect();
            Serial.println("WiFi connected! This WiFi network has been set as the default.");
        }
        else
        {
            Serial.println("Could not connect to the supplied WiFi network.");
        }
    }
    else
    {
        return CommandCode::COMMAND_NOT_FOUND;
    }
    return CommandCode::COMMAND_FOUND;
};