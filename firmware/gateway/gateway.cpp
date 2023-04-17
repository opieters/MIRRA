#include "gateway.h"
#include <SPIFFS.h>
#include <cstring>
#include "utilities.h"
#include <NTPClient.h>


const char nodeDataFN[] = "/nodes.dat";
const char sensorDataFN[] = "/data.dat";

Gateway::Gateway(Logger *log)
    : log{log},
      rtc{PCF2129_RTC(RTC_INT_PIN, RTC_ADDRESS)},
      lora{LoRaModule(&rtc, this->log, CS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, RX_PIN, TX_PIN)},
      wifi{WiFiClient()},
      mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, this->wifi)}
{
    if (firstBoot)
    {
        File nodesFile = SPIFFS.open(nodeDataFN, FILE_WRITE, true);
        nodesFile.write((size_t)0);
        nodesFile.close();
        File dataFile = SPIFFS.open(sensorDataFN, FILE_WRITE, true);
        dataFile.close();
    }

    nodesFromFile();
}

void Gateway::nodesFromFile()
{
    File nodesFile = SPIFFS.open(nodeDataFN, FILE_READ);
    size_t size;
    nodesFile.read((uint8_t*)&size, sizeof(size_t));
    nodes.resize(size);
    nodesFile.read((uint8_t*)nodes.data(), size * sizeof(Node));
    nodesFile.close();
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
        esp_sleep_enable_timer_wakeup(SECONDS_TO_US(sleep_time));
    }
    else
    {
        // We use the external RTC
        rtc.enable_alarm();
        rtc.write_alarm_epoch(rtc.read_time_epoch() + (uint32_t)round(sleep_time));

        esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc.getIntPin(), 0);
    }
    esp_sleep_enable_ext1_wakeup((gpio_num_t)BOOT_PIN, ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    esp_deep_sleep_start();
}

void Gateway::deepSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc.read_time_epoch();
    deepSleep((float)(time - ctime));
}

void Gateway::discovery()
{
    if (nodes.size() >= MAX_SENSOR_NODES)
    {
        log->print(Logger::Level::info, "Could not run discovery because maximum amount of nodes has been reached.");
        return;
    }
    log->print(Logger::Level::info, "Sending discovery message.");
    lora.sendMessage(Message(Message::Type::HELLO, lora.getMACAddress(), MACAddress::broadcast));
    Message hello_reply = lora.receiveMessage(DISCOVERY_TIMEOUT, Message::Type::HELLO_REPLY);
    if (hello_reply.isType(Message::Type::ERROR))
    {
        log->print(Logger::Level::error, "Error while receiving reply to discovery message. Aborting discovery.");
        return;
    }
    log->printf(Logger::Level::info, "Node found at %s", hello_reply.getSource().toString());

    uint32_t ctime = rtc.read_time_epoch();
    uint32_t sample_time = ((ctime + SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
    uint32_t comm_time = nodes.empty() ? ctime + COMMUNICATION_INTERVAL: nodes.back().getNextCommTime() + COMMUNICATION_PERIOD_LENGTH + COMMUNICATION_PERIOD_PADDING ;
    
    lora.sendMessage(TimeConfigMessage(lora.getMACAddress(), hello_reply.getSource(), ctime, sample_time, SAMPLING_INTERVAL, comm_time, COMMUNICATION_INTERVAL));
    Message time_ack = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::Type::ACK_TIME, TIME_CONFIG_ATTEMPTS, hello_reply.getSource());
    if (time_ack.isType(Message::Type::ERROR))
    {
        log->printf(Logger::Level::error, "Error while receiving ack to time config message from %s. Aborting discovery.", hello_reply.getSource().toString());
        return;
    }

    log->printf(Logger::Level::info, "Registering node %s", time_ack.getSource());
    Node new_node = Node(time_ack.getSource(), ctime, comm_time);
    storeNode(new_node);

}

void Gateway::storeNode(Node& n)
{
    nodes.push_back(n);
    File nodesFile = SPIFFS.open(nodeDataFN, FILE_APPEND);
    nodesFile.write(nodes.size());
    nodesFile.write((uint8_t*)nodes.data(), nodes.size());
    nodesFile.close();
}

bool Gateway::printDataUART()
{
    // connect to MQTT server

    if (!SPIFFS.exists(sensorDataFN))
    {
        Serial.println("No measurement data file found.");
        return false;
    }

    sensorData = SPIFFS.open(sensorDataFN, FILE_READ);
    sensorData.seek(nBytesRead);

    Serial.print("Moving start to");
    Serial.println(nBytesRead);

    // upload all the data to the server
    // the data is stored as:
    // ... | MAC sensor node (6) | timestamp (4) |  n values (1) | sensor id (1) | sensor value (4) | sensor id (1) | sensor value (4) | ... | MAC sensor node (6)
    // the MQTT server can only process one full sensor readout at a time, so we read part per part
    while (nBytesRead < nBytesWritten)
    {
        uint8_t buffer[256];
        uint8_t buffer_length = 0;

        // read the MAC address, timestamp and number of readouts
        nBytesRead += sensorData.read(buffer, MACAddress::length + sizeof(uint32_t) + sizeof(uint8_t));
        buffer_length += MACAddress::length + sizeof(uint32_t) + sizeof(uint8_t);

        uint8_t nSensorValues = buffer[buffer_length - 1];

        // read data to buffer
        nBytesRead += sensorData.read(&buffer[buffer_length], nSensorValues * (sizeof(float) + sizeof(uint8_t)));
        buffer_length += nSensorValues * (sizeof(float) + sizeof(uint8_t));

        Serial.print("SENSOR DATA [");

        for (size_t i = 0; i < buffer_length; i++)
        {
            Serial.print(buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println("]");
    }
    Serial.flush();

    sensorData.close();

    return true;
}

bool Gateway::uploadData()
{

    // connect to MQTT server
    uint8_t maxNAttempts = 5;
    char clientid[MACAddress::length];
    strcpy(clientid, lora.getMACAddress().toString());
    mqtt.connect(clientid);
    Serial.print("Connecting to MQTT server");
    while (!mqtt.connected())
    {
        mqtt.connect(clientid);
        Serial.print(".");
        delay(1000);
        if (maxNAttempts == 0)
        {
            Serial.println();
            Serial.println("Could not connect to MQTT server");
            return false;
        }
        maxNAttempts--;
    }

    Serial.println(" Connected.");
    Serial.flush();

    if (!SPIFFS.exists(sensorDataFN))
    {
        Serial.println("No measurement data file found.");
        mqtt.disconnect();
        return false;
    }

    sensorData = SPIFFS.open(sensorDataFN, FILE_READ);
    sensorData.seek(nBytesRead);

    Serial.print("Moving start to");
    Serial.println(nBytesRead);

    // upload all the data to the server
    // the data is stored as:
    // ... | MAC sensor node (6) | timestamp (4) |  n values (1) | sensor id (1) | sensor value (4) | ... | MAC sensor node (6)
    // the MQTT server can only process one full sensor readout at a time, so we read part per part
    while (nBytesRead < nBytesWritten)
    {
        uint8_t buffer[256];
        uint8_t buffer_length = 0;
        char topicHeader[sizeof(TOPIC_PREFIX) + (6 * 2 + 5) * 2 + 2 + 10];

        // read the MAC address, timestamp and number of readouts
        nBytesRead += sensorData.read(buffer, MACAddress::length + sizeof(uint32_t) + sizeof(uint8_t));
        buffer_length += MACAddress::length + sizeof(uint32_t) + sizeof(uint8_t);

        uint8_t nSensorValues = buffer[buffer_length - 1];
        Serial.print("Reading ");
        Serial.print(nSensorValues);
        Serial.println(" sensor values.");

        // read data to buffer
        nBytesRead += sensorData.read(&buffer[buffer_length], nSensorValues * (sizeof(float) + sizeof(uint8_t)));
        buffer_length += nSensorValues * (sizeof(float) + sizeof(uint8_t));

        // create MQTT message

        // the topic includes the sensor MAC
        MACAddress sensor_mac = MACAddress(buffer);
        createTopic(topicHeader, ARRAY_LENGTH(topicHeader), sensor_mac);

        // make sure we are still connected
        while (!mqtt.connected())
        {
            mqtt.connect(clientid);
            delay(10);
        };

        Serial.println("Topic header:");
        Serial.println(topicHeader);

        Serial.println("Data:");
        for (size_t i = 0; i < buffer_length - MACAddress::length; i++)
        {
            Serial.print(buffer[MACAddress::length + i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.flush();
        delay(1000);

        // send the MQTT data
        mqtt.publish(topicHeader, &buffer[MACAddress::length], buffer_length - MACAddress::length, false);
        wifi.flush();

        delay(1000);
    }

    Serial.println("Sent all data. Disconnecting.");
    mqtt.disconnect();

    sensorData.close();

    SPIFFS.remove(sensorDataFN);
    nBytesRead = 0;
    nBytesWritten = 0;

    return true;
}

char *Gateway::createTopic(char *buffer, size_t max_len, MACAddress& nodeMAC)
{
    char mac_string_buffer[MACAddress::string_length];
    snprintf(buffer, max_len, "%s/%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString(), nodeMAC.toString(mac_string_buffer));
    return buffer;
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
        log->print(Logger::debug,".");
    }
    log->print(Logger::info, " done.");
    log->printf(Logger::info, "Date and time: %s %s", timeClient.getFormattedDate(), timeClient.getFormattedTime());

    timeClient.end();
    return timeClient.getEpochTime();
}


void Gateway::storeSensorData(SensorDataMessage& m, File& dataFile)
{
    uint8_t buffer[SensorDataMessage::max_length];
    m.to_data(buffer);
    buffer[0] = 0; // mark not uploaded (yet)
    dataFile.write(buffer, m.getLength());
}