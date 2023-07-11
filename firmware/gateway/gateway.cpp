#include "gateway.h"
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

Gateway::Gateway(const MIRRAPins& pins)
    : MIRRAModule(MIRRAModule::start(pins)), mqttClient{WiFiClient()}, mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, mqttClient)}
{
    if (initialBoot)
    {
        log.print(Logger::info, "First boot.");

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
    log.printf(Logger::info, "Used %uKB of %uKB available on flash.", LittleFS.usedBytes() / 1000, LittleFS.totalBytes() / 1000);
    nodesFromFile();
}

void Gateway::wake()
{
    log.print(Logger::debug, "Running wake()...");
    if (!nodes.empty() && rtc.read_time_epoch() >= WAKE_COMM_PERIOD(nodes[0].getNextCommTime()))
        commPeriod();
    // send data to server only every UPLOAD_EVERY comm periods
    if (commPeriods >= UPLOAD_EVERY)
    {
        uploadPeriod();
    }
    Serial.printf("Welcome! This is Gateway %s\n", lora.getMACAddress().toString());
    Commands(this).prompt();
    log.print(Logger::debug, "Entering deep sleep...");
    if (nodes.empty())
        deepSleep(COMMUNICATION_INTERVAL);
    else
        deepSleepUntil(WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

void Gateway::discovery()
{
    if (nodes.size() >= MAX_SENSOR_NODES)
    {
        log.print(Logger::info, "Could not run discovery because maximum amount of nodes has been reached.");
        return;
    }
    log.print(Logger::info, "Sending discovery message.");
    lora.sendMessage(Message<HELLO>(lora.getMACAddress(), MACAddress::broadcast));
    log.print(Logger::debug, "Awaiting discovery response message ...");
    auto helloReply{lora.receiveMessage<HELLO_REPLY>(DISCOVERY_TIMEOUT)};
    if (!helloReply)
    {
        log.print(Logger::error, "Error while awaiting/receiving reply to discovery message. Aborting discovery.");
        return;
    }
    log.printf(Logger::info, "Node found at %s", helloReply->getSource().toString());

    uint32_t cTime{rtc.read_time_epoch()};
    uint32_t sampleTime{((cTime + SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING};
    uint32_t commTime{nodes.empty() ? cTime + COMMUNICATION_INTERVAL
                                    : nodes.back().getNextCommTime() + COMMUNICATION_PERIOD_LENGTH + COMMUNICATION_PERIOD_PADDING};

    log.printf(Logger::debug, "Sending time config message to %s ...", helloReply->getSource().toString());
    auto timeConfig{Message<TIME_CONFIG>(lora.getMACAddress(), helloReply->getSource(), cTime, sampleTime, SAMPLING_INTERVAL, commTime, COMMUNICATION_INTERVAL,
                                         COMMUNICATION_PERIOD_LENGTH)};
    lora.sendMessage(timeConfig);
    auto time_ack{lora.receiveMessage<ACK_TIME>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, helloReply->getSource())};
    if (!time_ack)
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Aborting discovery.", helloReply->getSource().toString());
        return;
    }

    log.printf(Logger::info, "Registering node %s", time_ack->getSource().toString());
    nodes.push_back(Node(timeConfig));
    updateNodesFile();
}

void Gateway::nodesFromFile()
{
    log.print(Logger::debug, "Recovering nodes from file...");
    File nodesFile = LittleFS.open(NODES_FP);
    uint8_t size = nodesFile.read();
    log.printf(Logger::debug, "%u nodes found in %s", size, NODES_FP);
    nodes.resize(size);
    nodesFile.read((uint8_t*)nodes.data(), size * sizeof(Node));
    nodesFile.close();
}

void Gateway::updateNodesFile()
{
    File nodesFile = LittleFS.open(NODES_FP, "w");
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
    log.print(Logger::info, "Starting comm period...");
    std::vector<Message<SENSOR_DATA>> data;
    data.reserve(MAX_SENSOR_MESSAGES * nodes.size());
    for (Node& n : nodes) // naively assume that every node's comm time is properly ordered : this would change the moment the comm interval is changed AND a
                          // node misses its new time config
    {
        if (!nodeCommPeriod(n, data))
            n.naiveTimeConfig();
    }
    if (nodes.empty())
    {
        log.print(Logger::info, "No comm periods performed because no nodes have been registered.");
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
    uint32_t ctime = rtc.read_time_epoch();
    if (ctime > n.getNextCommTime())
    {
        log.printf(Logger::error, "Node %s's comm time was faultily scheduled before this gateway's comm period. Skipping communication with this node.",
                   n.getMACAddress().toString());
        return false;
    }
    lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
    uint32_t listen_ms = COMMUNICATION_PERIOD_PADDING * 1000; // pre-listen in anticipation of message
    size_t messages_received = 0;
    while (true)
    {
        log.printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
        auto sensorData{lora.receiveMessage<SENSOR_DATA>(SENSOR_DATA_TIMEOUT, SENSOR_DATA_ATTEMPTS, n.getMACAddress(), listen_ms)};
        listen_ms = 0;
        if (!sensorData)
        {
            log.printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping communication with this node.", n.getMACAddress().toString());
            return false;
        }
        log.printf(Logger::info, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData->getLength());
        data.push_back(*sensorData);
        messages_received++;
        if (sensorData->isLast() || messages_received >= MAX_SENSOR_MESSAGES)
        {
            log.print(Logger::debug, "Last message received.");
            break;
        }
        log.printf(Logger::debug, "Sending data ACK to %s ...", n.getMACAddress().toString());
        lora.sendMessage(Message<ACK_DATA>(lora.getMACAddress(), n.getMACAddress()));
    }
    ctime = rtc.read_time_epoch();
    uint32_t commTime = n.getNextCommTime() + COMMUNICATION_INTERVAL;
    log.printf(Logger::info, "Sending time config message to %s ...", n.getMACAddress().toString());
    auto timeConfig{Message<TIME_CONFIG>(lora.getMACAddress(), n.getMACAddress(), ctime, 0, SAMPLING_INTERVAL, commTime, COMMUNICATION_INTERVAL,
                                         COMMUNICATION_PERIOD_LENGTH)};
    lora.sendMessage(timeConfig);
    auto timeAck = lora.receiveMessage<ACK_TIME>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
    if (!timeAck)
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Skipping communication with this node.",
                   n.getMACAddress().toString());
        return false;
    }
    log.printf(Logger::info, "Communication with node %s successful: %u messages received", n.getMACAddress().toString(), messages_received);
    n.timeConfig(timeConfig);
    return true;
}

void Gateway::wifiConnect(const char* SSID, const char* password)
{
    log.printf(Logger::info, "Connecting to WiFi with SSID: %s", SSID);
    WiFi.begin(SSID, password);
    for (size_t i = 10; i > 0 && WiFi.status() != WL_CONNECTED; i--)
    {
        delay(1000);
        Serial.print('.');
    }
    Serial.print('\n');
    if (WiFi.status() != WL_CONNECTED)
    {
        log.print(Logger::error, "Could not connect to WiFi.");
        return;
    }
    log.print(Logger::info, "Connected to WiFi.");
    strncpy(ssid, SSID, sizeof(ssid));
    strncpy(pass, password, sizeof(pass));
}

void Gateway::wifiConnect() { wifiConnect(ssid, pass); }

uint32_t Gateway::getWiFiTime()
{
    WiFiUDP ntpUDP;
    NTPClient timeClient{ntpUDP, NTP_URL, 3600, 60000};
    timeClient.begin();

    log.print(Logger::info, "Fetching NTP time");
    while (!timeClient.update())
    {
        delay(500);
    }
    timeClient.end();
    return timeClient.getEpochTime();
}

void Gateway::rtcUpdateTime()
{
    wifiConnect();
    if (WiFi.status() == WL_CONNECTED)
    {
        log.print(Logger::debug, "Writing time to RTC...");
        rtc.write_time_epoch(getWiFiTime());
        log.print(Logger::info, "RTC time updated.");
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
    log.printf(Logger::info, "Commencing upload to MQTT server...");
    wifiConnect();
    if (WiFi.status() != WL_CONNECTED)
    {
        log.print(Logger::error, "WiFi not connected. Aborting upload to MQTT server...");
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
                    log.printf(Logger::debug, "MQTT message succesfully published.");
                    // mark uploaded
                    size_t curPos{data.position()};
                    data.seek(curPos - size);
                    data.write(1);
                    data.seek(curPos);
                    messagesPublished++;
                }
                else
                {
                    log.printf(Logger::error, "Error while publishing to MQTT server. State: %i", mqtt.state());
                    nErrors++;
                }
            }
            else
            {
                log.printf(Logger::error, "Error while connecting to MQTT server. Aborting upload. State: %i", mqtt.state());
                upload = false;
            }
        }
        if (nErrors >= MAX_MQTT_ERRORS)
        {
            log.print(Logger::error, "Too many errors while publishing to MQTT server. Aborting upload.");
            upload = false;
        }
    }
    mqtt.disconnect();
    log.printf(Logger::info, "MQTT upload finished with %u messages sent.", messagesPublished);
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