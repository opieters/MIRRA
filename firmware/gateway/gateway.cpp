#include "gateway.h"
#include <cstring>

void Node::timeConfig(TimeConfigMessage &m)
{
    last_comm_time = m.getCTime();
    next_sample_time = (m.getSampleTime() == 0) ? next_sample_time : m.getSampleTime();
    sample_interval = (m.getSampleInterval() == 0) ? sample_interval : m.getSampleInterval();
    next_comm_time = m.getCommTime();
    comm_interval = m.getCommInterval();
    comm_duration = m.getCommDuration();
}

void Node::naiveTimeConfig()
{
    next_comm_time += comm_interval;
}

const char sensorDataTempFN[] = "/data_temp.dat";

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
    }
    Serial.printf("Welcome! This is Gateway %s", lora.getMACAddress().toString());
    enterCommandPhase();
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
        uint8_t ssid_buffer_length = Serial.readBytesUntil('\n', ssid_buffer, sizeof(ssid_buffer));
        ssid_buffer[ssid_buffer_length] = '\0';
        if (ssid_buffer[ssid_buffer_length - 1] == '\r')
            ssid_buffer[ssid_buffer_length - 1] = '\0';
        Serial.println(ssid_buffer);
        Serial.println("Enter WiFi password:");
        uint8_t pass_buffer_length = Serial.readBytesUntil('\n', pass_buffer, sizeof(pass_buffer));
        pass_buffer[pass_buffer_length] = '\0';
        if (pass_buffer[pass_buffer_length - 1] == '\r')
            pass_buffer[pass_buffer_length - 1] = '\0';
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
    Message hello_reply = lora.receiveMessage<Message>(DISCOVERY_TIMEOUT, Message::HELLO_REPLY);
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
    TimeConfigMessage timeConfig(TimeConfigMessage(lora.getMACAddress(), hello_reply.getSource(), ctime, sample_time, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL, COMMUNICATION_PERIOD_LENGTH));
    lora.sendMessage(timeConfig);
    Message time_ack = lora.receiveMessage<Message>(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, hello_reply.getSource());
    if (time_ack.isType(Message::ERROR))
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Aborting discovery.", hello_reply.getSource().toString());
        return;
    }

    log.printf(Logger::info, "Registering node %s", time_ack.getSource().toString());
    Node new_node = Node(timeConfig);
    nodes.push_back(new_node);
    updateNodesFile();
}

void Gateway::nodesFromFile()
{
    log.print(Logger::debug, "Recovering nodes from file...");
    File nodesFile = SPIFFS.open(NODES_FP, FILE_READ);
    uint8_t size = nodesFile.read();
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
    for (Node &n : nodes) // naively assume that every node's comm time is properly ordered : this would change the moment the comm interval is changed AND a node misses its new time config
    {

        if (!nodeCommPeriod(n, dataFile))
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
    commPeriods++;
    dataFile.close();
    dataFile = SPIFFS.open(DATA_FP, FILE_READ);
    pruneSensorData(dataFile);
    dataFile.close();
}

bool Gateway::nodeCommPeriod(Node &n, File &dataFile)
{
    uint32_t ctime = rtc.read_time_epoch();
    if (ctime > n.getNextCommTime())
    {
        log.printf(Logger::error, "Node %s's comm time was faultily scheduled before this gateway's comm period. Skipping communication with this node.", n.getMACAddress().toString());
        return false;
    }
    lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
    uint32_t listen_ms = COMMUNICATION_PERIOD_PADDING * 1000; // pre-listen in anticipation of message
    for (size_t i = 0; i < MAX_SENSOR_MESSAGES; i++)
    {
        log.printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
        SensorDataMessage sensorData = lora.receiveMessage<SensorDataMessage>(SENSOR_DATA_TIMEOUT, Message::SENSOR_DATA, SENSOR_DATA_ATTEMPTS, n.getMACAddress(), listen_ms);
        listen_ms = 0;
        if (sensorData.isType(Message::ERROR))
        {
            log.printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping communication with this node.", n.getMACAddress().toString());
            return false;
        }
        log.printf(Logger::info, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData.getLength());
        storeSensorData(sensorData, dataFile);
        if (sensorData.isLast() || i == (MAX_SENSOR_MESSAGES - 1))
        {
            log.print(Logger::debug, "Last message received.");
            break;
        }
        log.printf(Logger::debug, "Sending data ACK to %s ...", n.getMACAddress().toString());
        lora.sendMessage(Message(Message::DATA_ACK, lora.getMACAddress(), n.getMACAddress()));
    }

    uint32_t comm_time = n.getNextCommTime() + COMMUNICATION_INTERVAL;
    log.printf(Logger::info, "Sending time config message to %s ...", n.getMACAddress().toString());
    TimeConfigMessage timeConfig = TimeConfigMessage(lora.getMACAddress(), n.getMACAddress(), ctime, 0, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL, COMMUNICATION_PERIOD_LENGTH);
    lora.sendMessage(timeConfig);
    Message time_ack = lora.receiveMessage<Message>(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
    if (time_ack.isType(Message::ERROR))
    {
        log.printf(Logger::error, "Error while receiving ack to time config message from %s. Skipping communication with this node.", n.getMACAddress().toString());
        return false;
    }
    log.printf(Logger::info, "Communication with node %s successful.", n.getMACAddress().toString());
    n.timeConfig(timeConfig);
    return true;
}

void Gateway::pruneSensorData(File &dataFile)
{
    size_t fileSize = dataFile.size();
    if (fileSize <= MAX_SENSORDATA_FILESIZE)
        return;
    File dataFileTemp = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    while (dataFile.available())
    {
        uint8_t message_length = sizeof(message_length) + dataFile.peek();
        if (fileSize > MAX_SENSORDATA_FILESIZE)
        {
            fileSize -= message_length;
            dataFile.seek(message_length, fs::SeekCur); // skip over the next message
        }
        else
        {
            uint8_t buffer[message_length];
            dataFile.read(buffer, message_length);
            dataFileTemp.write(buffer, message_length);
        }
    }
    dataFileTemp.flush();
    log.printf(Logger::info, "Sensor data pruned from %u bytes to %u bytes.", fileSize, dataFileTemp.size());
    dataFileTemp.close();
    SPIFFS.remove(DATA_FP);
    SPIFFS.rename(sensorDataTempFN, DATA_FP);
}

void Gateway::wifiConnect(const char *SSID, const char *password)
{
    log.printf(Logger::info, "Connecting to WiFi with SSID: %s", SSID);
    WiFi.begin(SSID, pass);
    for (size_t i = 10; i > 0 && WiFi.status() != WL_CONNECTED; i--)
    {
        delay(1000);
        Serial.print('.');
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

// (TOPIC_PREFIX)/(MAC address gateway)/(MAC address node)
char *Gateway::createTopic(char *topic, MACAddress const &nodeMAC)
{
    char mac_string_buffer[MACAddress::string_length];
    snprintf(topic, topic_size, "%s/%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString(), nodeMAC.toString(mac_string_buffer));
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
    size_t n_errors = 0; // amount of errors while uploading
    bool upload = true;
    File rdata = SPIFFS.open(DATA_FP, FILE_READ);
    File wdata = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    while (rdata.available())
    {
        uint8_t size = rdata.read();
        uint8_t buffer[size];
        rdata.read(buffer, size);
        if (buffer[0] == 0 && upload) // upload flag: not yet uploaded
        {
            SensorDataMessage message(buffer);
            char topic[topic_size];
            createTopic(topic, message.getSource());

            if (!mqtt.connected())
                mqtt.connect(lora.getMACAddress().toString());
            if (mqtt.connected() && mqtt.publish(topic, &buffer[Message::header_length], message.getLength() - Message::header_length))
            {
                log.printf(Logger::debug, "MQTT message succesfully published.");
                buffer[0] = 1; // mark uploaded
            }
            else
            {
                log.printf(Logger::error, "Error while connecting/publishing to MQTT server. State:  %i", mqtt.state());
                n_errors++;
            }
        }
        if (n_errors >= MAX_MQTT_ERRORS)
            upload = false;
        wdata.write(size);
        wdata.write(buffer, size);
    }
    commPeriods = 0;
    rdata.close();
    wdata.close();
    SPIFFS.remove(DATA_FP);
    SPIFFS.rename(sensorDataTempFN, DATA_FP);
    rdata = SPIFFS.open(DATA_FP, FILE_READ);
    pruneSensorData(rdata);
    rdata.close();
}