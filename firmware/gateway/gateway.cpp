#include "gateway.h"
#include <cstring>

const char sensorDataTempFN[] = "/data_temp.dat";
extern volatile bool commandPhaseEntry;

RTC_DATA_ATTR bool initialBoot = true;
RTC_DATA_ATTR int commPeriods = 0;

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
        File nodesFile = SPIFFS.open(NODES_FP, FILE_WRITE, true);
        nodesFile.write(0);
        nodesFile.close();
        if (SPIFFS.exists(DATA_FP))
            SPIFFS.remove(DATA_FP);
        File dataFile = SPIFFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();

        log.print(Logger::info, "Retrieving time from WiFi....");
        wifiConnect();
        if (WiFi.status() == WL_CONNECTED)
        {
            log.print(Logger::debug, "Writing time to RTC...");
            rtc.write_time_epoch(getWiFiTime());
            log.print(Logger::debug, "Sending test MQQT message...");
        }
        initialBoot = false;
        commPeriods = 0;
    }

    nodesFromFile();
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
    log.print(Logger::info, "Press the BOOT pin to enter command phase ...");
    for (size_t i = 0; i < UART_PHASE_ENTRY_PERIOD * 10; i++)
    {
        if (commandPhaseEntry)
        {
            log.print(Logger::info, "Entering command phase...");
            commandPhase();
            break;
        }
        delay(100);
    }
    log.print(Logger::debug, "Entering deep sleep...");
    if (nodes.empty())
        deepSleep(COMMUNICATION_INTERVAL);
    else
        deepSleepUntil(WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

MIRRAModule::CommandCode Gateway::processCommands(char *command)
{
    return MIRRAModule::processCommands(command);
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
    File nodesFile = SPIFFS.open(NODES_FP, FILE_APPEND);
    nodesFile.write(nodes.size());
    nodesFile.write((uint8_t *)nodes.data(), nodes.size());
    nodesFile.close();
}

void Gateway::commPeriod()
{
    File dataFile = SPIFFS.open(DATA_FP, FILE_APPEND);
    for (Node n : nodes) // naively assume that every node's comm time is properly ordered : this would change the moment the comm interval is changed AND a node misses its new time config
    {
        lightSleepUntil(LISTEN_COMM_PERIOD(n.getNextCommTime())); // light sleep until scheduled comm period
        log.printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
        Message sensorDataM = lora.receiveMessage(SENSOR_DATA_TIMEOUT + COMMUNICATION_PERIOD_PADDING, Message::SENSOR_DATA, SENSOR_DATA_ATTEMPTS, n.getMACAddress());
        if (sensorDataM.isType(Message::ERROR))
        {
            log.printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping communication with this node.", n.getMACAddress().toString());
            continue;
        }
        SensorDataMessage sensorData = *static_cast<SensorDataMessage *>(&sensorDataM);
        log.printf(Logger::debug, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData.getLength());
        storeSensorData(sensorData, dataFile);

        uint32_t ctime = rtc.read_time_epoch();
        uint32_t comm_time = ctime + COMMUNICATION_INTERVAL;

        lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), n.getMACAddress(), 0, 0, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
        Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
        if (time_ack.isType(Message::ERROR))
        {
            log.printf(Logger::error, "Error while receiving ack to time config message from %s. Skipping communication with this node.", n.getMACAddress().toString());
            continue;
        }
        n.updateLastCommTime(ctime);
        n.updateNextCommTime(comm_time);
    }
    if (nodes.empty())
    {
        log.print(Logger::info, "No comm periods performed because no nodes have been registered.");
    }
    dataFile.close();
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

void Gateway::wifiConnect()
{
    log.printf(Logger::info, "Connecting to WiFi with SSID: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
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
}

uint32_t Gateway::getWiFiTime(void)
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

void Gateway::uploadPeriod()
{
    wifiConnect();
    char topic_array[sizeof(TOPIC_PREFIX) + 1 + MACAddress::string_length + 1]; // TOPIC_PREFIX + '/' + GATEWAY MAC
    snprintf(topic_array, 36, "%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString());
    WiFi.disconnect();
}
