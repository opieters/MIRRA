#include "gateway.h"
#include <cstring>

const char sensorDataTempFN[] = "/data_temp.dat";
extern volatile bool commandPhaseFlag;

RTC_DATA_ATTR bool initialBoot = true;
RTC_DATA_ATTR int commPeriods = 0;

RTC_DATA_ATTR char ssid[32] = WIFI_SSID;
RTC_DATA_ATTR char pass[32] = WIFI_PASS;

Gateway::Gateway(const MIRRAPins &pins)
    : MIRRAModule(MIRRAModule::start(pins)),
      mqtt_client{WiFiClient()},
      mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, mqtt_client)}
{
    if (initialBoot)
    {
        log.print(Logger::info, "First boot.");

        // manage filesystem
        if (SPIFFS.exists(NODES_FP))
            SPIFFS.remove(NODES_FP);
        updateNodesFile();
        if (SPIFFS.exists(DATA_FP))
            SPIFFS.remove(DATA_FP);
        File dataFile = SPIFFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();

        rtcUpdateTime();
        initialBoot = false;
    }

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
        commPeriods = 0;
    }
    commandPhase();
    log.print(Logger::debug, "Entering deep sleep...");
    if (nodes.empty())
        deepSleep(COMMUNICATION_INTERVAL);
    else
        deepSleepUntil(WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

MIRRAModule::CommandCode Gateway::processCommands(char *command)
{
    CommandCode code = MIRRAModule::processCommands(command);
    if (code != CommandCode::COMMAND_NOT_FOUND)
        return code;
    if (strcmp(command, "discovery") == 0)
    {
        discovery();
        return CommandCode::COMMAND_FOUND;
    }
    else if (strcmp(command, "rtc") == 0)
    {
        rtcUpdateTime();
        return CommandCode::COMMAND_FOUND;
    }
    else if (strcmp(command, "wifi") == 0)
    {
        char ssid_buffer[sizeof(ssid)];
        char pass_buffer[sizeof(pass)];
        Serial.println("Enter WiFi SSID:");
        ssid_buffer[Serial.readBytesUntil('\n', ssid_buffer, sizeof(ssid_buffer))] = '\0';
        Serial.println(ssid_buffer);
        Serial.println("Enter WiFi password:");
        pass_buffer[Serial.readBytesUntil('\n', pass_buffer, sizeof(pass_buffer))] = '\0';
        Serial.println(pass_buffer);
        wifiConnect(ssid_buffer, pass_buffer);
        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.disconnect();
            Serial.println("WiFi connected! This WiFi network has been set as the default.");
        }
        else
        {
            Serial.println("Could not connect to the supplied WiFi network.");
        }
        return CommandCode::COMMAND_FOUND;
    }
    return CommandCode::COMMAND_NOT_FOUND;
};

void Gateway::discovery()
{
    if (nodes.size() >= MAX_SENSOR_NODES)
    {
        log.print(Logger::info, "Could not run discovery because maximum amount of nodes has been reached.");
        return;
    }
    log.print(Logger::info, "Sending discovery message.");
    lora.sendMessage(Message(Message::HELLO, lora.getMACAddress(), MACAddress::broadcast));
    log.print(Logger::debug, "Awaiting discovery response message ...");
    Message hello_reply = lora.receiveMessage(DISCOVERY_TIMEOUT, Message::HELLO_REPLY);
    if (hello_reply.isType(Message::ERROR))
    {
        log.print(Logger::error, "Error while awaiting/receiving reply to discovery message. Aborting discovery.");
        return;
    }
    log.printf(Logger::info, "Node found at %s", hello_reply.getSource().toString());

    uint32_t ctime = rtc.read_time_epoch();
    uint32_t sample_time = ((ctime + SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
    uint32_t comm_time = nodes.empty() ? ctime + COMMUNICATION_INTERVAL : nodes.back().getNextCommTime() + COMMUNICATION_PERIOD_LENGTH + COMMUNICATION_PERIOD_PADDING;

    log.printf(Logger::debug, "Sending time config message to %s ...", hello_reply.getSource().toString());
    lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), hello_reply.getSource(), ctime, sample_time, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
    Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, hello_reply.getSource());
    if (time_ack.isType(Message::ERROR))
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Aborting discovery.", hello_reply.getSource().toString());
        return;
    }

    log.printf(Logger::info, "Registering node %s", time_ack.getSource());
    Node new_node = Node(time_ack.getSource(), ctime, comm_time);
    storeNode(new_node);
}

void Gateway::storeNode(Node &n)
{
    nodes.push_back(n);
    updateNodesFile();
}

void Gateway::nodesFromFile()
{
    log.print(Logger::debug, "Recovering nodes from file...");
    File nodesFile = SPIFFS.open(NODES_FP, FILE_READ);
    uint8_t size;
    nodesFile.read(&size, sizeof(uint8_t));
    log.printf(Logger::debug, "%u nodes found in %s", size, NODES_FP);
    nodes.resize(size);
    nodesFile.read((uint8_t *)nodes.data(), size * sizeof(Node));
    nodesFile.close();
}

void Gateway::updateNodesFile()
{
    File nodesFile = SPIFFS.open(NODES_FP, FILE_WRITE);
    nodesFile.write(nodes.size());
    nodesFile.write((uint8_t *)nodes.data(), nodes.size() * sizeof(Node));
    nodesFile.close();
}

void Gateway::printNodes()
{
    for (Node n : nodes)
    {
        Serial.printf("NODE MAC: %s, LAST COMM TIME: %u, NEXT COMM TIME: %u\n", n.getMACAddress().toString(), n.getLastCommTime(), n.getNextCommTime());
    }
}

void Gateway::commPeriod()
{
    log.print(Logger::info, "Starting comm period...");
    File dataFile = SPIFFS.open(DATA_FP, FILE_APPEND);
    for (Node n : nodes) // naively assume that every node's comm time is properly ordered : this would change the moment the comm interval is changed AND a node misses its new time config
    {
        nodeCommPeriod(n, dataFile);
    }
    if (nodes.empty())
    {
        log.print(Logger::info, "No comm periods performed because no nodes have been registered.");
    }
    dataFile.close();
}

void Gateway::nodeCommPeriod(Node &n, File &dataFile)
{
    uint32_t ctime = rtc.read_time_epoch();
    if (ctime > n.getNextCommTime())
    {
        log.printf(Logger::error, "Node %s's comm time was faultily scheduled before this gateway's comm period. Skipping communication with this node.", n.getMACAddress().toString());
        return;
    }
    lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
    log.printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
    Message sensorDataM = lora.receiveMessage(SENSOR_DATA_TIMEOUT, Message::SENSOR_DATA, SENSOR_DATA_ATTEMPTS, n.getMACAddress(), COMMUNICATION_PERIOD_PADDING);
    if (sensorDataM.isType(Message::ERROR))
    {
        log.printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping communication with this node.", n.getMACAddress().toString());
        return;
    }
    SensorDataMessage sensorData = *static_cast<SensorDataMessage *>(&sensorDataM);
    log.printf(Logger::debug, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData.getLength());
    storeSensorData(sensorData, dataFile);

    while (!sensorData.isLast())
    {
        log.printf(Logger::debug, "Sending data ACK to %s ...", n.getMACAddress().toString());
        lora.sendMessage(Message(Message::DATA_ACK, lora.getMACAddress(), n.getMACAddress()));
        log.printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
        sensorDataM = lora.receiveMessage(SENSOR_DATA_TIMEOUT, Message::SENSOR_DATA, SENSOR_DATA_ATTEMPTS, n.getMACAddress());
        if (sensorDataM.isType(Message::ERROR))
        {
            log.printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping further communication with this node.", n.getMACAddress().toString());
            return;
        }
        sensorData = *static_cast<SensorDataMessage *>(&sensorDataM);
        log.printf(Logger::debug, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData.getLength());
        storeSensorData(sensorData, dataFile);
    }

    uint32_t comm_time = n.getNextCommTime() + COMMUNICATION_INTERVAL;
    log.printf(Logger::debug, "Sending time config message to %s ...", n.getMACAddress().toString());
    lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), n.getMACAddress(), ctime, 0, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
    Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
    if (time_ack.isType(Message::ERROR))
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Skipping communication with this node.", n.getMACAddress().toString());
        return;
    }
    n.updateCommTime(ctime, comm_time);
}

void Gateway::storeSensorData(SensorDataMessage &m, File &dataFile)
{
    uint8_t buffer[SensorDataMessage::max_length];
    m.to_data(buffer);
    buffer[0] = 0; // mark not uploaded (yet)
    dataFile.write((uint8_t)m.getLength());
    dataFile.write(buffer, m.getLength());
}

void Gateway::pruneSensorData(File &dataFile)
{
    size_t fileSize = dataFile.size();
    if (fileSize < MAX_SENSORDATA_FILESIZE)
        return;
    File dataFileTemp = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    while (dataFile.peek() != EOF)
    {
        uint8_t message_length = sizeof(message_length) + dataFile.peek();
        if (fileSize > MAX_SENSORDATA_FILESIZE)
        {
            fileSize -= message_length;
            dataFile.seek(message_length, fs::SeekCur);
        }
        else
        {
            uint8_t buffer[message_length];
            dataFile.read(buffer, message_length);
            dataFileTemp.write(buffer, message_length);
        }
    }
    dataFileTemp.close();
    SPIFFS.remove(DATA_FP);
    SPIFFS.rename(sensorDataTempFN, DATA_FP);
}

char *Gateway::createTopic(char *buffer, size_t max_len, MACAddress &nodeMAC)
{
    char mac_string_buffer[MACAddress::string_length];
    snprintf(buffer, max_len, "%s/%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString(), nodeMAC.toString(mac_string_buffer));
    return buffer;
}

void Gateway::wifiConnect(const char *SSID, const char *password)
{
    log.printf(Logger::info, "Connecting to WiFi with SSID: %s", SSID);
    WiFi.begin(SSID, pass);
    for (size_t i = 10; i > 0 && WiFi.status() != WL_CONNECTED; i--)
    {
        delay(1000);
        log.print(Logger::info, ".");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        log.print(Logger::error, "Could not connect to WiFi.");
        return;
    }
    log.print(Logger::info, "Connected to WiFi.");
    strncpy(ssid, SSID, sizeof(ssid));
    strncpy(pass, password, sizeof(pass));
}

void Gateway::wifiConnect()
{
    wifiConnect(ssid, pass);
}

uint32_t Gateway::getWiFiTime()
{
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, NTP_URL, 3600, 60000);
    timeClient.begin();

    log.print(Logger::info, "Fetching UTP time");
    while (!timeClient.update())
    {
        delay(500);
        log.print(Logger::debug, ".");
    }
    log.print(Logger::info, "done.");
    log.printf(Logger::info, "Date and time: %s %s", timeClient.getFormattedDate(), timeClient.getFormattedTime());

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

void Gateway::uploadPeriod()
{
    wifiConnect();
    char topic_array[sizeof(TOPIC_PREFIX) + 1 + MACAddress::string_length + 1]; // TOPIC_PREFIX + '/' + GATEWAY MAC
    snprintf(topic_array, 36, "%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString());
    WiFi.disconnect();
}

void Node::updateCommTime(uint32_t last_comm_time, uint32_t next_comm_time)
{
    this->last_comm_time = last_comm_time;
    this->next_comm_time = next_comm_time;
}

void Node::updateSampleTime(uint32_t sample_interval, uint32_t next_sample_time)
{
    this->sample_interval = sample_interval;
    this->next_sample_time = next_sample_time;
}