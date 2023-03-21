/****************************************************
LIBRARIES
****************************************************/

// #define USE_WIFI

#include "config.h"
#include "utilities.h"
#include "Radio.h"
#include "UplinkModule.h"
#include "PubSubClient.h"
#include "Storage.h"
#include "PCF2129_RTC.h" //!< External RTC module
#include <cstring>
#include "SPIFFS.h"
#ifdef USE_WIFI
#include "WiFi.h"
#endif
#include "SensorNode.h"
#include "gateway.h"
#include "CommunicationCommon.h"

#define USE_WIFI

// Pin assignments
#define SS_PIN 13   // Slave select pin
#define RST_PIN 15  // Reset pin
#define DIO0_PIN 34 // DIO0 pin: LoRa interrupt pin
#define DIO1_PIN 4  // DIO1 pin: not used??
#define TX_POWER 17
#define RX_POWER 16

PCF2129_RTC rtc(RTC_INT_PIN, RTC_ADDRESS);
RadioModule radioModule(&rtc, SS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, RX_POWER, TX_POWER);

// iPad
//const char *ssid = "Eline's iPhone";
//const char *password = "shotspot";

// Toren
const char* ssid = "GontrodeWiFi2";
const char* password = "b5uJeswA";

#define TOPIC_PREFIX "fornalab" //!< MQTT topic = `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`

#ifdef USE_WIFI
WiFiClient espClient;
#endif

const auto mqttServer = IPAddress(193, 190, 127, 143); //(85,119,83,194); //IPAddress(193,190,127,143);

#ifdef USE_WIFI
PubSubClient mqtt(mqttServer, 1883, espClient);
#else
TinyGsmClient client(*gsmModule.modem);
PubSubClient mqtt(mqttServer, 1883, client);
#endif

#ifdef DEBUG_TIMING
constexpr uint32_t discoveryInterval = 60;       // seconds
constexpr uint32_t discoverySendInterval = 1000; // ms
constexpr uint32_t shortDiscoverySendInterval = 10;
#else
constexpr uint32_t discoveryInterval = 1 * 60;   // seconds
constexpr uint32_t discoverySendInterval = 1000; // ms
constexpr uint32_t shortDiscoverySendInterval = 10;
#endif

RTC_DATA_ATTR uint32_t uploadDataTime;
RTC_DATA_ATTR uint32_t sampleTime;
RTC_DATA_ATTR uint32_t readoutTime;

extern SensorNode_t sensorNodes[20];
extern uint8_t nSensorNodes;

extern bool firstBoot;

extern File sensorData;

constexpr uint8_t sda_pin = 21, scl_pin = 22;

constexpr uint32_t nextReadoutThreshold = 15; // seconds

constexpr float maxSensorPeriod = 2 * 60 * 60; // max 2h between sensor readouts

#ifdef USE_WIFI
Gateway gateway(&radioModule, &rtc, &mqtt, &espClient, nullptr);
#else
Gateway gateway(&radioModule, &rtc, &mqtt, nullptr, &gsmModule);
#endif

LoRaMessage rxm;
LoRaMessage txm;
bool status = false;

void updateSampleReadoutTimes(void)
{
    uint32_t ctime = rtc.read_time_epoch();
    // update sample time to next sample event
    while (sampleTime <= ctime)
    {
        sampleTime += sampleInterval;
    }
    while (readoutTime <= ctime)
    {
        readoutTime += communicationInterval;
    }
}

void runDiscovery(uint32_t timeout)
{
    int i = 0;
    while (rtc.read_time_epoch() < timeout)
    {
        // make sure we only sample and communicate in the future
        updateSampleReadoutTimes();

        // send discovery message
        Serial.print("Sending discovery message ");
        Serial.println(i + 1);

        gateway.createDiscoveryMessage(txm);
        radioModule.sendMessage(txm);

        // receive reply messages
        status = radioModule.receiveAnyMessage(discoverySendInterval, rxm);

        if (status && (rxm.getLength() == 7) && (rxm.getData()[6] == CommunicationCommandToInt(CommunicationCommand::HELLO_REPLY)))
        {
            SensorNode_t *node = gateway.addNewNode(rxm);
            if (node != nullptr)
            {
                gateway.createUpdateMessage(*node, txm);
                radioModule.sendMessage(txm);
            }
        }

        // if all node loctions are full, stop discovery
        if (nSensorNodes == ARRAY_LENGTH(sensorNodes))
        {
            break;
        }

        i++;
    }
}

bool dump_uart_log = false;
void IRAM_ATTR gpio_0_isr_callback()
{
        dump_uart_log = true;

}

void setup(void)
{
    Serial.begin(115200);
    Serial2.begin(9600);
    Wire.begin(sda_pin, scl_pin, 100000U);
    Serial.println("Initialising...");
    Serial.flush();

    pinMode(0, INPUT);
    attachInterrupt(0, gpio_0_isr_callback, FALLING);
    
    delay(1000);

    if (!SPIFFS.begin(true))
    {
        Serial.println("Mounting SPIFFS failed");
        SPIFFS.format();
        ESP.restart();
    }

    // open the file with the sensor data
    if (!openSensorDataFile())
    {
        Serial.println("There was an error with the data file.");
        // gateway.deepSleep(maxSensorPeriod);
        SPIFFS.format();
        ESP.restart();
    }

    selectModule(ModuleSelector::LoRaModule);
    radioModule.init();
    rtc.begin();

#ifndef USE_WIFI
    gsmModule.reset();
#endif

    if (firstBoot)
    {
#ifdef USE_WIFI
        WiFi.begin(ssid, password);
#else
        gsmModule.startGPRS();
#endif

        Serial.println("First boot. Fetching time.");

        Serial.flush();

        selectModule(ModuleSelector::LoRaModule);

#ifdef USE_WIFI
        Serial.print("Connecting to WiFi");
        uint16_t i = 500;
        while ((i > 0) && (WiFi.status() != WL_CONNECTED))
        {
            delay(500);
            Serial.print(".");
            i--;
        }
        Serial.println();

        Serial.print("Connected to WiFi network at IP: ");
        Serial.println(WiFi.localIP());
        rtc.write_time_epoch(gateway.getWiFiTime());
#else
        gsmModule.GPRSConnectAPN();
        rtc.write_time_epoch(gateway.getGSMTime());
#endif

        delay(100);

        sampleTime = rtc.read_time_epoch();
        sampleTime = sampleTime / 60 / 60;
        sampleTime = (sampleTime) * 60 * 60;
        sampleTime += sampleInterval;

        readoutTime = sampleTime;
        readoutTime += communicationInterval + sampleInterval / 2;

        // upload immediately after readout
        uploadDataTime = readoutTime;

        gateway.initFirstBoot();

        firstBoot = false;

        WiFi.disconnect();
    }

    updateSampleReadoutTimes();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
    {
        gpio_0_isr_callback();
    }
}

void loop(void)
{
    size_t i;

    status = radioModule.receiveAnyMessage(receiveDataWindow, rxm);

    if(dump_uart_log || (digitalRead(0) == LOW)){
        dump_uart_log = false;

        Serial.println("Data in memory:");
        sensorData = openDataFile();
        if (sensorData)
        {
            size_t n_bytes_read = 0;
            size_t n_bytes_read_old = 0;
            size_t read_length;
            size_t i;

            uint8_t data_buffer[10];
            while (n_bytes_read < sensorData.size())
            {
                n_bytes_read_old = n_bytes_read;
                n_bytes_read = min(n_bytes_read + 10, sensorData.size());
                read_length = n_bytes_read - n_bytes_read_old;

                sensorData.read(data_buffer, read_length);

                char print_buffer[10];
                for (i = 0; i < read_length; i++)
                {
                    sprintf(print_buffer, "%02x", data_buffer[i]);
                    Serial.print(print_buffer);
                }
                Serial.println();
            }

            sensorData.close();
        }
        else
        {
            Serial.println("There was an error.");
        }

        Serial.println("Done dumping data.");
        Serial.flush();
    }

    if (!status || rxm.getLength() < 7)
    {
        return;
    }

    uint8_t *data = rxm.getData();

    switch (data[6])
    {
    case CommunicationCommand::HELLO_REPLY:
    {
        SensorNode_t *node = gateway.addNewNode(rxm);
        if (node != nullptr)
        {
            gateway.createUpdateMessage(*node, txm);
            radioModule.sendMessage(txm);
        }
    }
    break;
    case CommunicationCommand::TIME_CONFIG:
        break;
    case CommunicationCommand::MEASUREMENT_DATA:
    {
        Serial.println("Receiving sensor data.");

        SensorNode_t *node = nullptr;
        // find sensor node
        for (int i = 0; i < nSensorNodes; i++)
        {
            node = &sensorNodes[i];
            if (!radioModule.checkRecipient(rxm, node->macAddresss))
                node = nullptr;
            else
                break;
        }

        if (node == nullptr)
        {
            Serial.println("Node not found.");
            break;
        }

        Serial.print("Node ");
        Serial.print(": ");
        for (int j = 0; j < 6; j++)
        {
            Serial.print(node->macAddresss[j], HEX);
            Serial.print(" ");
        }
        Serial.println();

        auto time = rtc.read_time_epoch();
        node->lastCommunicationTime = time;

        // store data
        gateway.storeSensorData(rxm);

        updateSampleReadoutTimes();
        SensorNodeUpdateTimes(node);
        while(node->nextCommunicationTime <= time){
            node->nextCommunicationTime += communicationInterval;
            }
Serial.println("Next comms");
            Serial.println(node->nextCommunicationTime);


        // send update message on next communication interval
        gateway.createUpdateMessage(*node, txm);
        radioModule.sendMessage(txm);

        status = radioModule.receiveMessage(receiveAckWindow, rxm, node->macAddresss, CommunicationCommand::ACK_DATA);
        // wait for ack
        if (!status)
        {
            Serial.println("No ACK received on updated timing message.");
            SensorNodeLogError(node);
        }
    }
    break;
    case CommunicationCommand::ACK_DATA:
    break;
    case CommunicationCommand::REQUEST_MEASUREMENT_DATA:
    break;
    case CommunicationCommand::HELLO:
        break;
    case CommunicationCommand::NONE:
    default:
        break;
    }


}