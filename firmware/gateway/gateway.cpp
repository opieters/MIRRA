#include "gateway.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include <cstring>

void Node::timeConfig(Message<TIME_CONFIG>& m)
{
    this->sampleInterval = m.getSampleInterval();
    this->sampleRounding = m.getSampleRounding();
    this->sampleOffset = m.getSampleOffset();
    this->lastCommTime = m.getCTime();
    this->commInterval = m.getCommInterval();
    this->nextCommTime = m.getCommTime();
    this->maxMessages = m.getMaxMessages();
    if (this->errors > 0)
        this->errors--;
}

void Node::naiveTimeConfig()
{
    this->nextCommTime += commInterval;
    this->errors++;
}

RTC_DATA_ATTR bool initialBoot{true};
RTC_DATA_ATTR int commPeriods{0};

RTC_DATA_ATTR char ssid[32]{WIFI_SSID};
RTC_DATA_ATTR char pass[32]{WIFI_PASS};

RTC_DATA_ATTR uint32_t defaultSampleInterval{DEFAULT_SAMPLE_INTERVAL};
RTC_DATA_ATTR uint32_t defaultSampleRounding{DEFAULT_SAMPLE_ROUNDING};
RTC_DATA_ATTR uint32_t defaultSampleOffset{DEFAULT_SAMPLE_OFFSET};
RTC_DATA_ATTR uint32_t commInterval{DEFAULT_COMM_INTERVAL};

// TODO: Instead of using this lambda to determine if a node is lost, use a bool stored in each node that
// signifies if a node is ' well-scheduled ', implying both that the node is not lost and that it follows
// the scheduled comm time of the previous node tightly (i.e., without scheduling gaps). Removal of a node
// would imply loss of well-scheduledness of all following nodes, and changing of the comm period would imply
// loss of well-scheduledness for all nodes.
// Currently only the latter portion of this functionality is available with just the isLost lambda,
// During the comm period, a node that is not 'well-scheduled' would be scheduled so that it would become so.

auto lambdaIsLost = [](const Node& e) { return e.getCommInterval() != commInterval; };

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
    nodesFromFile();
}

void Gateway::wake()
{
    Log::debug("Running wake()...");
    if (!nodes.empty() && rtc.getSysTime() >= WAKE_COMM_PERIOD(nodes[0].getNextCommTime()))
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
        deepSleep(commInterval);
    else
        deepSleepUntil(WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

std::optional<std::reference_wrapper<Node>> Gateway::macToNode(char* mac)
{
    MACAddress searchMac{MACAddress::fromString(mac)};
    for (Node& n : nodes)
    {
        if (searchMac == n.getMACAddress())
            return std::make_optional(std::ref(n));
    }
    Log::error("No node found for ", mac);
    return std::nullopt;
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

    uint32_t cTime{rtc.getSysTime()};
    uint32_t sampleInterval{defaultSampleInterval}, sampleRounding{defaultSampleRounding}, sampleOffset{defaultSampleOffset};
    uint32_t commTime{std::all_of(nodes.cbegin(), nodes.cend(), lambdaIsLost) ? cTime + commInterval : nextScheduledCommTime()};

    Message<TIME_CONFIG> timeConfig{lora.getMACAddress(),
                                    helloReply->getSource(),
                                    cTime,
                                    sampleInterval,
                                    sampleRounding,
                                    sampleOffset,
                                    commInterval,
                                    commTime,
                                    MAX_MESSAGES(commInterval, sampleInterval)};
    Log::debug("Time config constructed. cTime = ", cTime, " sampleInterval = ", sampleInterval, " sampleRounding = ", sampleRounding,
               " sampleOffset = ", sampleOffset, " commInterval = ", commInterval, " comTime = ", commTime);
    Log::debug("Sending time config message to ", helloReply->getSource().toString());
    lora.sendMessage(timeConfig);
    auto time_ack{lora.receiveMessage<ACK_TIME>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, helloReply->getSource())};
    if (!time_ack)
    {
        Log::error("Error while receiving ack to time config message from ", helloReply->getSource().toString(), ". Aborting discovery.");
        return;
    }

    Log::info("Registering node ", time_ack->getSource().toString());
    nodes.emplace_back(timeConfig);
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

void Gateway::commPeriod()
{
    Log::info("Starting comm period...");
    std::vector<Message<SENSOR_DATA>> data;
    size_t expectedMessages{0};
    for (const Node& n : nodes)
        expectedMessages += n.getMaxMessages();
    data.reserve(expectedMessages);
    auto lambdaByNextCommTime = [](const Node& a, const Node& b) { return a.getNextCommTime() < b.getNextCommTime(); };
    std::sort(nodes.begin(), nodes.end(), lambdaByNextCommTime);
    uint32_t farCommTime = -1;
    for (Node& n : nodes)
    {
        if (n.getNextCommTime() > farCommTime)
            break;
        farCommTime = n.getNextCommTime() + 2 * (COMM_PERIOD_LENGTH(MAX_MESSAGES(commInterval, n.getSampleInterval())) + COMM_PERIOD_PADDING);
        if (!nodeCommPeriod(n, data))
            n.naiveTimeConfig();
    }
    std::sort(nodes.begin(), nodes.end(), lambdaByNextCommTime);
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

uint32_t Gateway::nextScheduledCommTime()
{
    for (size_t i{1}; i <= nodes.size(); i++)
    {
        const Node& n{nodes[nodes.size() - i]};
        if (!lambdaIsLost(n))
            return n.getNextCommTime() + COMM_PERIOD_LENGTH(n.getMaxMessages()) + COMM_PERIOD_PADDING;
    }
    Log::error("Next scheduled comm time was asked but all nodes are lost!");
    return -1;
}

bool Gateway::nodeCommPeriod(Node& n, std::vector<Message<SENSOR_DATA>>& data)
{
    uint32_t cTime{static_cast<uint32_t>(rtc.getSysTime())};
    if (cTime > n.getNextCommTime())
    {
        Log::error("Node ", n.getMACAddress().toString(),
                   "'s comm time was faultily scheduled before this gateway's comm period. Skipping communication with this node.");
        return false;
    }
    lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
    uint32_t listenMs{COMM_PERIOD_PADDING * 1000};            // pre-listen in anticipation of message
    size_t messagesReceived{0};
    while (true)
    {
        Log::debug("Awaiting data from ", n.getMACAddress().toString(), " ...");
        auto sensorData{lora.receiveMessage<SENSOR_DATA>(SENSOR_DATA_TIMEOUT, SENSOR_DATA_ATTEMPTS, n.getMACAddress(), listenMs)};
        listenMs = 0;
        if (!sensorData)
        {
            Log::error("Error while awaiting/receiving data from ", n.getMACAddress().toString(), ". Skipping communication with this node.");
            return false;
        }
        Log::info("Sensor data received from ", n.getMACAddress().toString(), " with length ", sensorData->getLength());
        data.push_back(*sensorData);
        messagesReceived++;
        if (sensorData->isLast() || messagesReceived >= n.getMaxMessages())
        {
            Log::debug("Last message received.");
            break;
        }
        Log::debug("Sending data ACK to ", n.getMACAddress().toString(), " ...");
        lora.sendMessage(Message<ACK_DATA>(lora.getMACAddress(), n.getMACAddress()));
    }
    uint32_t commTime{n.getNextCommTime() + commInterval};
    if (lambdaIsLost(n) && !(std::all_of(nodes.cbegin(), nodes.cend(), lambdaIsLost)))
        commTime = nextScheduledCommTime();
    Log::info("Sending time config message to ", n.getMACAddress().toString(), " ...");
    cTime = rtc.getSysTime();
    Message<TIME_CONFIG> timeConfig{lora.getMACAddress(),
                                    n.getMACAddress(),
                                    cTime,
                                    n.getSampleInterval(),
                                    n.getSampleRounding(),
                                    n.getSampleOffset(),
                                    commInterval,
                                    commTime,
                                    MAX_MESSAGES(commInterval, n.getSampleInterval())};
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
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, NTP_URL);
        sntp_init();
        int64_t timeout{esp_timer_get_time() + 10 * 1000 * 1000};
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            if (esp_timer_get_time() > timeout)
            {
                Log::error("Failed to update system time within 10s timeout");
                WiFi.disconnect();
                return;
            }
        }
        if (sntp_enabled())
            sntp_stop();
        Log::debug("Writing time to RTC...");
        rtc.write_time_epoch(rtc.getSysTime());
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
    char macStringBuffer[MACAddress::stringLength];
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
            auto& message{Message<SENSOR_DATA>::fromData(buffer)};
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

void Gateway::parseUpdate(char* update)
{
    if (strlen(update) < (MACAddress::stringLength + 2 + 2 + 1))
    {
        Log::error("Update string '", update, "' has invalid length.");
        return;
    }
    char* macString{update};
    auto node{macToNode(macString)};
    if (!node)
    {
        Log::error("Could not deduce node from update.");
        return;
    }
    char* timeString{&update[MACAddress::stringLength + 1]};
    uint32_t sampleInterval, sampleRounding, sampleOffset;
    if (sscanf(timeString, "%u %u %u", &sampleInterval, &sampleRounding, &sampleOffset) != 3)
    {
        Log::error("Could not deduce updated timings from update string '", update, "'.");
        return;
    }
    node->get().setSampleInterval(sampleInterval);
    node->get().setSampleRounding(sampleRounding);
    node->get().setSampleOffset(sampleOffset);
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
        return changeWifi();
    }
    else if (strcmp(command, "printschedule") == 0)
    {
        printSchedule();
    }
    else
    {
        return CommandCode::COMMAND_NOT_FOUND;
    }
    return CommandCode::COMMAND_FOUND;
};

Gateway::Commands::CommandCode Gateway::Commands::changeWifi()
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
    return COMMAND_FOUND;
}

void Gateway::Commands::printSchedule()
{
    constexpr size_t timeLength{sizeof("0000-00-00 00:00:00")};
    char buffer[timeLength]{0};
    Serial.println("MAC\tNEXT COMM TIME\tSAMPLE INTERVAL\tMAX MESSAGES");
    for (const Node& n : parent->nodes)
    {
        tm time;
        time_t nextNodeCommTime{static_cast<time_t>(n.getNextCommTime())};
        gmtime_r(&nextNodeCommTime, &time);
        strftime(buffer, timeLength, "%F %T", &time);
        Serial.printf("%s\t%s\t%u\t%u\n", n.getMACAddress().toString(), buffer, n.getSampleInterval(), n.getMaxMessages());
    }
}