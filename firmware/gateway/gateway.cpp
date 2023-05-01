#include "gateway.h"
#include <SPIFFS.h>
#include <cstring>
#include "utilities.h"
#include <NTPClient.h>

const char sensorDataTempFN[] = "/data_temp.dat";
extern volatile bool commandPhaseEntry;

RTC_DATA_ATTR bool initialBoot = true;

Gateway::Gateway(Logger *log, PCF2129_RTC *rtc)
    : log{log},
      rtc{rtc},
      lora{LoRaModule(rtc, this->log, CS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, RX_PIN, TX_PIN)},
      mqtt_client{WiFiClient()},
      mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, this->mqtt_client)}
{
    rtc->begin();
    Serial.begin(115200);
    Serial2.begin(9600);
    Wire.begin(SDA_PIN, SCL_PIN, 100000U); // i2c
    log->print(Logger::info, "Initialising...");
    if (!SPIFFS.begin(true))
    {
        log->print(Logger::error, "Mounting SPIFFS failed! Restarting...");
        ESP.restart();
    }
    if (initialBoot)
    {
        log->print(Logger::info, "First boot.");

        // manage filesystem
        File nodesFile = SPIFFS.open(NODES_FP, FILE_WRITE, true);
        nodesFile.write((size_t)0);
        nodesFile.close();
        File dataFile = SPIFFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();

        // retrieve time
        wifiConnect();
        rtc->write_time_epoch(getWiFiTime());
        WiFi.disconnect();

        initialBoot = false;
    }

    nodesFromFile();
}

void Gateway::nodesFromFile()
{
    log->print(Logger::debug, "Recovering nodes from file...");
    File nodesFile = SPIFFS.open(NODES_FP, FILE_READ);
    size_t size;
    nodesFile.read((uint8_t *)&size, sizeof(size_t));
    nodes.resize(size);
    nodesFile.read((uint8_t *)nodes.data(), size * sizeof(Node));
    nodesFile.close();
}

void Gateway::wake()
{
    log->print(Logger::debug, "Running wake()...");
    if (!nodes.empty() && rtc->read_time_epoch() >= WAKE_COMM_PERIOD(nodes[0].getNextCommTime()))
        commPeriod();
    for (size_t i = 0; i < UART_PHASE_ENTRY_PERIOD * 10; i++)
    {
        if (commandPhaseEntry)
        {
            log->print(Logger::info, "Entering command phase...");
            commandPhase();
            break;
        }
        delay(100);
    }
    log->print(Logger::debug, "Entering deep sleep...");
    deepSleepUntil(nodes.empty() ? COMMUNICATION_INTERVAL : WAKE_COMM_PERIOD(nodes[0].getNextCommTime()));
}

void Gateway::commandPhase()
{
    Serial.println("COMMAND PHASE");
    char buffer[256];
    size_t length;
    Serial.setTimeout(UART_PHASE_TIMEOUT * 1000);
    while (true)
    {
        length = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        buffer[length] = '\0';
        if (strcmp(buffer, "") == 0 || strcmp(buffer, "exit") == 0 || strcmp(buffer, "close") == 0)
        {
            Serial.println("Exiting command phase...");
            return;
        }
        else if (strcmp(buffer, "discovery") == 0)
        {
            discovery();
        }
        else if (strcmp(buffer, "ls") == 0 || strcmp(buffer, "list") == 0)
        {
            listFiles();
        }
        else if (strncmp(buffer, "echo ", 5) == 0)
        {
            Serial.println(&buffer[5]);
        }
        else if (strncmp(buffer, "print ", 6) == 0)
        {
            printFile(&buffer[6]);
        }
        else
        {
            Serial.printf("Command '%s' not found or invalid argument(s) given.", buffer);
        }
    }
}

void Gateway::listFiles()
{
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        Serial.println(file.name());
        file = root.openNextFile();
    }
    root.close();
    file.close();
}

void Gateway::printFile(const char *filename)
{
    if (!SPIFFS.exists(filename))
    {
        Serial.printf("File '%s' does not exist.", filename);
        return;
    }
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file)
    {
        Serial.printf("Error while opening file '%s'", filename);
        return;
    }
    while (file.available())
    {
        Serial.write(file.read());
    }
    Serial.flush();
    file.close();
}

void Gateway::deepSleep(float sleep_time)
{
    if (sleep_time <= 0)
        return;

    lora.sleep();
    // For an unknown reason pin 15 was high by default, as pin 15 is connected to VPP with a 4.7k pull-up resistor it forced 3.3V on VPP when VPP was powered off.
    // Therefore we force pin 15 to a LOW state here.
    pinMode(15, OUTPUT);
    digitalWrite(15, LOW);

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep sleep
    if (sleep_time < 10)
    {
        // We use the internal timer
        esp_sleep_enable_timer_wakeup((uint64_t)(sleep_time * 1000 * 1000));
    }
    else
    {
        // We use the external RTC
        rtc->enable_alarm();
        rtc->write_alarm_epoch(rtc->read_time_epoch() + (uint32_t)round(sleep_time));

        esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc->getIntPin(), 0);
    }
    esp_sleep_enable_ext1_wakeup((gpio_num_t)BOOT_PIN, ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    esp_deep_sleep_start();
}

void Gateway::deepSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc->read_time_epoch();
    deepSleep((float)(time - ctime));
}

void Gateway::lightSleep(float sleep_time)
{
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)SECONDS_TO_US(sleep_time * 1000 * 1000));
    esp_light_sleep_start();
}

void Gateway::lightSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc->read_time_epoch();
    lightSleep((float)(time - ctime));
}

void Gateway::discovery()
{
    if (nodes.size() >= MAX_SENSOR_NODES)
    {
        log->print(Logger::info, "Could not run discovery because maximum amount of nodes has been reached.");
        return;
    }
    log->print(Logger::info, "Sending discovery message.");
    lora.sendMessage(Message(Message::HELLO, lora.getMACAddress(), MACAddress::broadcast));
    Message hello_reply = lora.receiveMessage(DISCOVERY_TIMEOUT, Message::HELLO_REPLY);
    if (hello_reply.isType(Message::ERROR))
    {
        log->print(Logger::error, "Error while awaiting/receiving reply to discovery message. Aborting discovery.");
        return;
    }
    log->printf(Logger::info, "Node found at %s", hello_reply.getSource().toString());

    uint32_t ctime = rtc->read_time_epoch();
    uint32_t sample_time = ((ctime + SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
    uint32_t comm_time = nodes.empty() ? ctime + COMMUNICATION_INTERVAL : nodes.back().getNextCommTime() + COMMUNICATION_PERIOD_LENGTH + COMMUNICATION_PERIOD_PADDING;

    lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), hello_reply.getSource(), ctime, sample_time, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
    Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, hello_reply.getSource());
    if (time_ack.isType(Message::ERROR))
    {
        log->printf(Logger::error, "Error while receiving ack to time config message from %s. Aborting discovery.", hello_reply.getSource().toString());
        return;
    }

    log->printf(Logger::info, "Registering node %s", time_ack.getSource());
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
        log->printf(Logger::debug, "Awaiting data from %s ...", n.getMACAddress().toString());
        Message sensorDataM = lora.receiveMessage(SENSOR_DATA_TIMEOUT + COMMUNICATION_PERIOD_PADDING, Message::SENSOR_DATA, SENSOR_DATA_ATTEMPTS, n.getMACAddress());
        SensorDataMessage sensorData = *static_cast<SensorDataMessage *>(&sensorDataM);
        if (sensorData.isType(Message::ERROR))
        {
            log->printf(Logger::error, "Error while awaiting/receiving data from %s. Skipping communication with this node.", n.getMACAddress().toString());
            continue;
        }
        log->printf(Logger::debug, "Sensor data received from %s with length %u.", n.getMACAddress().toString(), sensorData.getLength());
        storeSensorData(sensorData, dataFile);

        uint32_t ctime = rtc->read_time_epoch();
        uint32_t comm_time = ctime + COMMUNICATION_INTERVAL;

        lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), n.getMACAddress(), 0, 0, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
        Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::ACK_TIME, TIME_CONFIG_ATTEMPTS, n.getMACAddress());
        if (time_ack.isType(Message::ERROR))
        {
            log->printf(Logger::error, "Error while receiving ack to time config message from %s. Skipping communication with this node.", n.getMACAddress().toString());
            continue;
        }
        n.updateLastCommTime(ctime);
        n.updateNextCommTime(comm_time);
    }
    if (nodes.empty())
    {
        log->print(Logger::info, "No comm periods performed because no nodes have been registered.");
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
    if (dataFile.size() < MAX_SENSORDATA_FILESIZE)
        return;
    File dataFileTemp = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    size_t fileSize = dataFile.size();
    while (dataFile.peek() != EOF)
    {
        uint8_t message_length = dataFile.peek();
        if (fileSize > MAX_SENSORDATA_FILESIZE)
        {
            fileSize -= sizeof(message_length) + message_length;
            dataFile.seek(sizeof(message_length) + message_length, fs::SeekCur);
        }
        else
        {
            uint8_t buffer[sizeof(message_length) + message_length];
            dataFile.read(buffer, sizeof(message_length) + message_length);
            dataFileTemp.write(buffer, sizeof(message_length) + message_length);
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
    log->printf(Logger::info, "Connecting to WiFi with SSID: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (size_t i = 500; i > 0 && WiFi.status() != WL_CONNECTED; i--)
    {
        delay(500);
        log->print(Logger::info, ".");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        log->print(Logger::error, "Could not connect to WiFi.");
        return;
    }
    log->print(Logger::info, "Connected to WiFi.");
}

uint32_t Gateway::getWiFiTime(void)
{
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, NTP_URL, 3600, 60000);
    timeClient.begin();

    log->print(Logger::info, "Fetching UTP time");
    while (!timeClient.update())
    {
        delay(500);
        log->print(Logger::debug, ".");
    }
    log->print(Logger::info, " done.");
    log->printf(Logger::info, "Date and time: %s %s", timeClient.getFormattedDate(), timeClient.getFormattedTime());

    timeClient.end();
    return timeClient.getEpochTime();
}