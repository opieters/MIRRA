#include "gateway.h"
#include <SPIFFS.h>
#include <cstring>
#include "utilities.h"
#include <NTPClient.h>

RTC_DATA_ATTR size_t nBytesWritten = 0;
RTC_DATA_ATTR size_t nBytesRead = 0;

RTC_DATA_ATTR SensorNode_t sensorNodes[20];
RTC_DATA_ATTR uint8_t nSensorNodes = 0;

RTC_DATA_ATTR bool firstBoot = true;

const char nodeDataFN[] = "/nodes.dat";
const char sensorDataFN[] = "/data.dat";

File nodeData;
File sensorData;

const char *Gateway::topic = "fornalab";

extern uint32_t readoutTime, sampleTime;

Gateway::Gateway(Logger *log)
    : log{log},
      rtc{PCF2129_RTC(RTC_INT_PIN, RTC_ADDRESS)},
      lora{LoRaModule(&rtc, this->log, CS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, RX_PIN, TX_PIN)},
      wifi{WiFiClient()},
      mqtt{PubSubClient(MQTT_SERVER, MQTT_PORT, this->wifi)}
{
}

void Gateway::initFirstBoot(void)
{
    nSensorNodes = 0;

    if (openSensorDataFile())
    {
        nBytesWritten = sensorData.size();
        nBytesRead = sensorData.size();

        Serial.println("Set nbytes to values");
        Serial.print("Set nbytes written:");
        Serial.println(nBytesWritten);
        Serial.print("Set nbytes read:");
        Serial.println(nBytesRead);
        sensorData.close();
    }
    else
    {
        Serial.println("ERROR with FS.");
    }

    if (SPIFFS.exists(nodeDataFN))
    {
        nodeData = SPIFFS.open(nodeDataFN, FILE_READ);

        if (nodeData.available() > 0)
        {
            nSensorNodes = nodeData.read();
        }

        for (size_t i = 0; i < nSensorNodes; i++)
        {
            nodeData.read((uint8_t *)&sensorNodes[i], sizeof(SensorNode_t));
            SensorNodeUpdateTimes(&sensorNodes[i]);
        }

        // update values

        nodeData.close();

        if (nSensorNodes > 0)
        {
            Serial.print("Loaded ");
            Serial.print(nSensorNodes);
            Serial.println(" sensors from data storage.");
        }
        else
        {
            Serial.println("No sensor nodes found in memory.");
        }
    }
    else
    {
        nodeData = SPIFFS.open(nodeDataFN, FILE_WRITE);
        if (nodeData)
        {
            nodeData.write(0);
            nodeData.close();
            Serial.println("Sensor file not found.");
        }
        else
        {
            Serial.println("ERROR with FS.");
        }
    }
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
        mqtt->disconnect();
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
        char topicHeader[sizeof(topic) + (6 * 2 + 5) * 2 + 2 + 10];

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
        createTopic(topicHeader, ARRAY_LENGTH(topicHeader), &buffer[0]);

        // make sure we are still connected
        while (!mqtt->connected())
        {
            mqtt->connect(clientID);
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
        mqtt->publish(topicHeader, &buffer[MACAddress::length], buffer_length - MACAddress::length, false);

        if (espClient != nullptr)
        {
            espClient->flush();
        }

        delay(1000);
    }

    Serial.println("Sent all data. Disconnecting.");
    mqtt->disconnect();

    sensorData.close();

    SPIFFS.remove(sensorDataFN);
    nBytesRead = 0;
    nBytesWritten = 0;

    return true;
}

char *Gateway::createTopic(char *buffer, size_t max_len, MACAddress &nodeMAC)
{
    char mac_string_buffer[MACAddress::string_length];
    snprintf(buffer, max_len, "%s/%s/%s", TOPIC_PREFIX, lora.getMACAddress().toString(), nodeMAC.toString(mac_string_buffer));
    return buffer;
}

uint32_t Gateway::getWiFiTime(void)
{
    WiFiUDP ntpUDP;

    // NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
    NTPClient timeClient(ntpUDP, NTP_URL, 3600, 60000);

    timeClient.begin();

    Serial.print("Fetching UTP time");

    while (!timeClient.update())
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" done.");

    Serial.println("Date and time:");
    Serial.println(timeClient.getFormattedDate());
    Serial.println(timeClient.getFormattedTime());

    timeClient.end();

    return timeClient.getEpochTime();
}

uint32_t Gateway::getGSMTime()
{
    return uplinkModule->get_rtc_time();
}

SensorNode_t *Gateway::addNewNode(LoRaMessage &m)
{
    SensorNode_t *node = nullptr;
    if (nSensorNodes == ARRAY_LENGTH(sensorNodes))
    {
        Serial.println("Maximum number of sensor nodes reached.");
        return node;
    }

    MACAddress new_mac = MACAddress(m.getData());
    bool newNode = true;

    Serial.print("Adding node: ");
    Serial.println(new_mac.toString());

    // check if node in node list
    for (size_t j = 0; j < nSensorNodes; j++)
    {
        if (new_mac == MACAddress(sensorNodes[j].macAddresss))
        {
            newNode = false;
            node = &sensorNodes[j];
        }
    }

    // add new nodes to the node list
    if (newNode)
    {
        auto time = rtc->read_time_epoch();
        Serial.println("Adding new node.");

        SensorNodeInit(&sensorNodes[nSensorNodes], new_mac.get_address());
        node = &sensorNodes[nSensorNodes];
        nSensorNodes++;

        node->lastCommunicationTime = time;
        node->nextCommunicationTime = readoutTime + nSensorNodes * communicationSpacing;

        Serial.print("Added node: ");
        Serial.println(mac_str);

        // write node to file
        nodeData = SPIFFS.open(nodeDataFN, FILE_WRITE);
        nodeData.write(nSensorNodes);
        for (size_t j = 0; j < nSensorNodes; j++)
        {
            nodeData.write((uint8_t *)&sensorNodes[j], sizeof(SensorNode_t));
        }
        nodeData.close();
    }

    return node;
}

void Gateway::storeSensorData(LoRaMessage &m)
{
    size_t length = 0;

    uint8_t *macAddress = m.getData();
    length = 7; // skip MAC and command byte

    uint8_t nValues;

    Serial.print("N bytes written: ");
    Serial.println(nBytesWritten);
    Serial.print("N bytes read: ");
    Serial.println(nBytesRead);

    if (openSensorDataFile())
    {
        while (length < m.getLength())
        {
            // write the MAC address
            nBytesWritten += sensorData.write(macAddress, MACAddress::length);

            // write timestamp and number of values
            nBytesWritten += sensorData.write(&m.getData()[length], sizeof(uint32_t) + 1);
            length += sizeof(uint32_t) + 1;

            nValues = m.getData()[length - 1];

            Serial.print("N values received: ");
            Serial.println(nValues);

            nBytesWritten += sensorData.write(&m.getData()[length], nValues * (sizeof(float) + 1));
            length += nValues * (sizeof(float) + 1);
        }

        Serial.print("N bytes written: ");
        Serial.println(nBytesWritten);

        sensorData.close();
    }
    else
    {
        Serial.println("Unable to open file.");
    }
}

bool openSensorDataFile(void)
{
    if (SPIFFS.exists(sensorDataFN))
    {
        sensorData = SPIFFS.open(sensorDataFN, FILE_APPEND);
    }
    else
    {
        sensorData = SPIFFS.open(sensorDataFN, FILE_WRITE);
    }

    if (sensorData)
    {
        return true;
    }
    else
    {
        Serial.println("Unable to open data file.");
        return false;
    }
}