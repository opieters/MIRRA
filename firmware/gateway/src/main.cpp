/****************************************************
LIBRARIES
****************************************************/

//#define USE_WIFI

#include "config.h"
#include "utilities.h"
#include "Radio.h"
#include <TinyGsmClient.h>
#include "UplinkModule.h"
#include "PubSubClient.h"
#include "Storage.h"
#include "PCF2129_RTC.h"        //!< External RTC module
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
#define SS_PIN 13       // Slave select pin
#define RST_PIN 15      // Reset pin
#define DIO0_PIN 34      // DIO0 pin: LoRa interrupt pin
#define DIO1_PIN 4     // DIO1 pin: not used??
#define TX_POWER 17
#define RX_POWER 16

PCF2129_RTC rtc(RTC_INT_PIN, RTC_ADDRESS);
RadioModule radioModule(&rtc, SS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, RX_POWER, TX_POWER);

UplinkModule gsmModule(Serial2, 26);

const char* ssid = "procyon";
const char* password = "3WmU58fS";

#define TOPIC_PREFIX    "fornalab"  //!< MQTT topic = `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`


#ifdef USE_WIFI
WiFiClient espClient;
#endif

const auto mqttServer = IPAddress(193,190,127,143); //(85,119,83,194); //IPAddress(193,190,127,143);

#ifdef USE_WIFI
PubSubClient mqtt(mqttServer, 1883, espClient);
#else 
TinyGsmClient client(*gsmModule.modem);
PubSubClient mqtt(mqttServer, 1883, client);
#endif

#ifdef DEBUG_TIMING
constexpr uint32_t discoveryInterval = 60;       // seconds
constexpr uint32_t discoverySendInterval = 1000;    // ms
constexpr uint32_t shortDiscoverySendInterval = 10;
#else
constexpr uint32_t discoveryInterval = 10*60;       // seconds
constexpr uint32_t discoverySendInterval = 1000;    // ms
constexpr uint32_t shortDiscoverySendInterval = 10; // seconds
#endif

RTC_DATA_ATTR uint32_t uploadDataTime;
RTC_DATA_ATTR uint32_t sampleTime;
RTC_DATA_ATTR uint32_t readoutTime;

extern SensorNode_t sensorNodes[4];
extern uint8_t nSensorNodes;

extern GateWayState state;
extern GateWayState previousState;
extern bool firstBoot;

extern File sensorData;

constexpr uint8_t sda_pin = 21, scl_pin = 22;

constexpr uint32_t sleepThreshold = 5; // seconds

constexpr float maxSleepTime = 2*60*60; // sleep at most one hour

#ifdef USE_WIFI
Gateway gateway(&radioModule, &rtc, &mqtt, &espClient, nullptr);
#else
Gateway gateway(&radioModule, &rtc, &mqtt, nullptr, &gsmModule);
#endif

LoRaMessage rxm;
LoRaMessage txm;
bool status = false;

void updateSampleReadoutTimes(void){
    uint32_t ctime = rtc.read_time_epoch();
    // update sample time to next sample event
    while(sampleTime <= ctime){
        sampleTime += sampleInterval;
    }
    while(readoutTime <= ctime){
        readoutTime += communicationInterval;
    }
}

void runDiscovery(uint32_t timeout){
    int i = 0;
    while(rtc.read_time_epoch() < timeout){
        // make sure we only sample and communicate in the future
        updateSampleReadoutTimes();

        // send discovery message
        Serial.print("Sending discovery message ");
        Serial.println(i+1);

        gateway.createDiscoveryMessage(txm);
        radioModule.sendMessage(txm);

        // receive reply messages
        status = radioModule.receiveAnyMessage(discoverySendInterval, rxm);

        if(status 
            && (rxm.getLength() == 7) 
            && (rxm.getData()[6] == CommunicationCommandToInt(CommunicationCommand::HELLO_REPLY))){
            SensorNode_t* node = gateway.addNewNode(rxm);
            if(node != nullptr){
                gateway.createUpdateMessage(*node, txm);
                radioModule.sendMessage(txm);
            }
        }

        // if all node loctions are full, stop discovery
        if(nSensorNodes == ARRAY_LENGTH(sensorNodes)){
            break;
        }

        i++;
    }
}



void setup(void){
    Serial.begin(115200);
    Serial2.begin(9600);
    Wire.begin(sda_pin, scl_pin, 100000U);
    Serial.println("Initialising..."); 
    Serial.flush();

    delay(1000);

    if (!SPIFFS.begin(true)){
        Serial.println("Mounting SPIFFS failed");
        SPIFFS.format();
        ESP.restart(); 
    }

    // open the file with the sensor data
    if(!openSensorDataFile()){
        Serial.println("There was an error with the data file.");
        gateway.deepSleep(maxSleepTime);
        SPIFFS.format();
        ESP.restart(); 
    }

    selectModule(ModuleSelector::LoRaModule);
    radioModule.init();
    rtc.begin();

    #ifndef USE_WIFI
    gsmModule.reset();
    #endif

    if(firstBoot){
        #ifdef USE_WIFI
        WiFi.begin(ssid, password);
        #else
        gsmModule.startGPRS();
        #endif

        Serial.println("First boot. Fetching time."); 
        
        Serial.flush();

        selectModule(ModuleSelector::GPRSModule);

        #ifdef USE_WIFI
        Serial.print("Connecting to WiFi");
        uint16_t i = 500;
        while((i > 0) && (WiFi.status() != WL_CONNECTED)){
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
        sampleTime = (sampleTime+1) * 60 * 60;
        sampleTime += sampleInterval;

        readoutTime = sampleTime;
        readoutTime += communicationInterval + sampleInterval/2;

        // upload immediately after readout
        uploadDataTime = readoutTime;

        gateway.initFirstBoot();

        firstBoot = false;
    }

    updateSampleReadoutTimes();
}

void loop(void){
    size_t i;
    uint32_t timeout;
    
    switch(state){
        case GateWayState::READ_SENSORS:
            Serial.println("Receiving sensor data.");
            selectModule(ModuleSelector::LoRaModule);
            
            for(i = 0; i < nSensorNodes; i++){
                SensorNode_t* node = &sensorNodes[i];

                Serial.print("Node ");
                Serial.print(i);
                Serial.print(": ");
                for(int j = 0; j < 6; j++){
                    Serial.print(node->macAddresss[j], HEX);
                    Serial.print(" ");
                }
                Serial.println();

                gateway.createReadoutMessage(sensorNodes[i], txm);

                // wait until the right moment
                while(node->nextCommunicationTime > rtc.read_time_epoch()){
                    delay(100);
                }

                radioModule.sendMessage(txm);
                status = radioModule.receiveMessage(receiveDataWindow, rxm, node->macAddresss, CommunicationCommand::MEASUREMENT_DATA);
            
                if(status){
                    auto time = rtc.read_time_epoch();
                    node->lastCommunicationTime = time;

                    // store data
                    gateway.storeSensorData(rxm);

                    SensorNodeUpdateTimes(node);

                    // send update message on next communication interval
                    gateway.createUpdateMessage(*node, txm);
                    radioModule.sendMessage(txm);

                    status = radioModule.receiveMessage(receiveAckWindow, rxm, node->macAddresss, CommunicationCommand::ACK_DATA);
                    // wait for ack
                    if(!status){
                        Serial.println("No ACK received on updated timing message.");
                        SensorNodeLogError(node);
                    }
                    
                } else {
                    SensorNodeUpdateTimes(node);
                    SensorNodeLogError(node);
                }
            }

            // check if we go to sleep or upload the data to the server
            if(uploadDataTime < rtc.read_time_epoch()){
                state = GateWayState::UPLOAD_DATA;
            } else {
                state = GateWayState::START_SLEEP;
            }

            break;
        case GateWayState::UPLOAD_DATA:
            Serial.println("Uploading data to the server.");
            
            #ifdef USE_WIFI
            WiFi.begin(ssid, password);
            #else
            selectModule(ModuleSelector::GPRSModule);

            gsmModule.startGPRS();
            #endif

            gateway.uploadData(); 

            uploadDataTime += Gateway::getUploadDataInterval();

            state = GateWayState::MODULE_CHECK;
            break;
        case GateWayState::MODULE_CHECK:
            Serial.println("Checking for unclaimed modules.");
            selectModule(ModuleSelector::LoRaModule);   

            timeout = rtc.read_time_epoch() + shortDiscoverySendInterval;

            runDiscovery(timeout);
            state = GateWayState::START_SLEEP;

            break;
        case GateWayState::DISCOVER_MODULES:
            Serial.println("Running module discovery.");

            selectModule(ModuleSelector::LoRaModule);
            
            timeout = rtc.read_time_epoch() + discoveryInterval;

            runDiscovery(timeout);
            state = GateWayState::START_SLEEP;

            break;
        case GateWayState::ERROR:
            Serial.println("An unrecoverable error occurred.");
            while(1);
            break;

        case GateWayState::UART_READOUT:
            // TODO
            Serial.println("Data in memory:");
            sensorData = openDataFile();
            if(sensorData){
                size_t n_bytes_read = 0;
                size_t n_bytes_read_old = 0;
                size_t read_length;
                size_t i;

                uint8_t data_buffer[10];
                while(n_bytes_read < sensorData.size()){
                    n_bytes_read_old = n_bytes_read;
                    n_bytes_read = min(n_bytes_read + 10, sensorData.size());
                    read_length = n_bytes_read-n_bytes_read_old;

                    sensorData.read(data_buffer, read_length);

                    char print_buffer[10];
                    for(i = 0; i < read_length; i++){
                        sprintf(print_buffer, "%02x", data_buffer[i]);
                        Serial.print(print_buffer);
                    }
                    Serial.println();
                }

                sensorData.close();
            } else {
                Serial.println("There was an error.");
            }
            state = GateWayState::START_SLEEP;
            break;
        case GateWayState::START_SLEEP:
            Serial.println("Sleep state.");
            Serial.flush();

            if(nSensorNodes > 0){
                state = GateWayState::READ_SENSORS;

                float sleepTime = 2*maxSleepTime;
                
                for(i = 0; i < nSensorNodes; i++){
                    sleepTime = min(sleepTime, (float) (sensorNodes[0].nextCommunicationTime - rtc.read_time_epoch() - 3.0));
                }

                if(sleepTime < sleepThreshold){
                    break;
                }

                if(sleepTime > maxSleepTime){
                    Serial.print("Max sleep time exceeded. Limiting sleep time from ");
                    Serial.print(sleepTime);
                    Serial.print(" to ");
                    Serial.print(maxSleepTime);
                    Serial.println("s.");

                    sleepTime = maxSleepTime;
                    state = GateWayState::START_SLEEP;
                }

                if(sleepTime > 0){
                    Serial.print("Sleeping for ");
                    Serial.print(sleepTime);
                    Serial.println(" seconds.");
                    Serial.flush();
                    delay(1000);
                    Serial.flush();

                    gateway.deepSleep(sleepTime);
                }
            } else {
                state = GateWayState::DISCOVER_MODULES;
            }

            break;
    }

    // check if there are any incoming UART messages
    if(Serial.available() > 0){
        previousState = state;
        state = GateWayState::UART_READOUT;
    }
}
