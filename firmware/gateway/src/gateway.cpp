#include "gateway.h"
#include <SPIFFS.h>
#include <cstring>
#include "CommunicationCommon.h"
#include "utilities.h"
#include <NTPClient.h>
#include "WiFi.h"

RTC_DATA_ATTR size_t nBytesWritten = 0;
RTC_DATA_ATTR size_t nBytesRead = 0;

RTC_DATA_ATTR SensorNode_t sensorNodes[20];
RTC_DATA_ATTR uint8_t nSensorNodes = 0;

RTC_DATA_ATTR bool firstBoot = true;

const char nodeDataFN[11] = "/nodes.dat";
const char sensorDataFN[10] = "/data.dat";

File nodeData;
File sensorData;

const char* Gateway::topic = "fornalab";

extern uint32_t readoutTime, sampleTime;

Gateway::Gateway(RadioModule* lora, PCF2129_RTC* rtc, PubSubClient* mqtt, WiFiClient* espClient, UplinkModule* module) : radioModule(lora), rtc(rtc), mqtt(mqtt), espClient(espClient), uplinkModule(module) {
    uint8_t* macAddress = radioModule->getMACAddress();

    snprintf(clientID, ARRAY_LENGTH(clientID), "GateWay%02X%02X%02X%02X%02X%02X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
}

void Gateway::initFirstBoot(void){
    nSensorNodes = 0;
    
    if(openSensorDataFile()){
        nBytesWritten = sensorData.size();
        nBytesRead = sensorData.size();

        Serial.println("Set nbytes to values");
        Serial.print("Set nbytes written:");
        Serial.println(nBytesWritten);
        Serial.print("Set nbytes read:");
        Serial.println(nBytesRead);

        sensorData.close();

        
    } else {
        Serial.println("ERROR with FS.");
    }

    if(SPIFFS.exists(nodeDataFN)){
        nodeData = SPIFFS.open(nodeDataFN, FILE_READ);

        if(nodeData.available() > 0){
            nSensorNodes = nodeData.read();
        }

        for(size_t i = 0; i < nSensorNodes; i++){
            nodeData.read((uint8_t*) &sensorNodes[i], sizeof(SensorNode_t));
            SensorNodeUpdateTimes(&sensorNodes[i]);
        }

        // update values
        

        nodeData.close();

        if(nSensorNodes > 0){
            Serial.print("Loaded ");
            Serial.print(nSensorNodes);
            Serial.println(" sensors from data storage.");
        } else {
            Serial.println("No sensor nodes found in memory.");
        }
    } else {
        nodeData = SPIFFS.open(nodeDataFN, FILE_WRITE);
        if(nodeData){
            nodeData.write(0);
            nodeData.close();
            Serial.println("Sensor file not found.");
        } else {
            Serial.println("ERROR with FS.");
        }
    }
}

void Gateway::deepSleep(float sleep_time){
    selectModule(ModuleSelector::LoRaModule);
    radioModule->sleep();

    if(uplinkModule != nullptr){
        selectModule(ModuleSelector::GPRSModule);
        uplinkModule->sleep();
    }

    // For an unknown reason pin 15 was high by default, as pin 15 is connected to VPP with a 4.7k pull-up resistor it forced 3.3V on VPP when VPP was powered off.
    // Therefore we force pin 15 to a LOW state here.
    pinMode(15, OUTPUT); 
    digitalWrite(15, LOW); 

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); 

    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep sleep
    if (sleep_time < 10){
        // We use the internal timer
        esp_sleep_enable_timer_wakeup(SECONDS_TO_US(sleep_time));

        digitalWrite(16, LOW);
        esp_deep_sleep_start();
    } else {
        // We use the external RTC
        rtc->enable_alarm();
        uint32_t now = rtc->read_time_epoch();
        rtc->write_alarm_epoch(now + (uint32_t)round(sleep_time));

        digitalWrite(16, LOW);

        esp_sleep_enable_ext0_wakeup((gpio_num_t) rtc->getIntPin(), 0);
        esp_sleep_enable_ext1_wakeup((gpio_num_t) (1 << 0), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed

        esp_deep_sleep_start();
    }
}

bool Gateway::printDataUART(){
    // connect to MQTT server
    
    if(!SPIFFS.exists(sensorDataFN)){
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
    while(nBytesRead < nBytesWritten){
        uint8_t buffer[256];
        uint8_t buffer_length = 0;

        // read the MAC address, timestamp and number of readouts
        nBytesRead += sensorData.read(buffer, MAC_NUM_BYTES + sizeof(uint32_t) + sizeof(uint8_t));
        buffer_length += MAC_NUM_BYTES + sizeof(uint32_t) + sizeof(uint8_t);

        uint8_t nSensorValues = buffer[buffer_length-1];

        // read data to buffer
        nBytesRead += sensorData.read(&buffer[buffer_length], nSensorValues*(sizeof(float) + sizeof(uint8_t)));
        buffer_length += nSensorValues*(sizeof(float) + sizeof(uint8_t));

        Serial.print("SENSOR DATA [");

        for(size_t i = 0; i < buffer_length; i++){
            Serial.print(buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println("]");
    }
    Serial.flush();

    sensorData.close();

    return true;
}

bool Gateway::uploadData() {

    // connect to MQTT server
    uint8_t maxNAttempts = 5;
    mqtt->connect(clientID);
    Serial.print("Connecting to MQTT server");
    while (!mqtt->connected())
    {
        mqtt->connect(clientID);
        Serial.print(".");
        delay(1000);
        if (maxNAttempts == 0) {
            Serial.println();
            Serial.println("Could not connect to MQTT server");
            return false;
        }
        maxNAttempts--;
    }

    Serial.println(" Connected.");
    Serial.flush();
    
    if(!SPIFFS.exists(sensorDataFN)){
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
    while(nBytesRead < nBytesWritten){
        uint8_t buffer[256];
        uint8_t buffer_length = 0;
        char topicHeader[sizeof(topic)+(6*2+5)*2+2+10];

        // read the MAC address, timestamp and number of readouts
        nBytesRead += sensorData.read(buffer, MAC_NUM_BYTES + sizeof(uint32_t) + sizeof(uint8_t));
        buffer_length += MAC_NUM_BYTES + sizeof(uint32_t) + sizeof(uint8_t);

        uint8_t nSensorValues = buffer[buffer_length-1];
        Serial.print("Reading ");
        Serial.print(nSensorValues);
        Serial.println(" sensor values.");

        // read data to buffer
        nBytesRead += sensorData.read(&buffer[buffer_length], nSensorValues*(sizeof(float) + sizeof(uint8_t)));
        buffer_length += nSensorValues*(sizeof(float) + sizeof(uint8_t));

        // create MQTT message

        // the topic includes the sensor MAC
        createTopic(topicHeader, ARRAY_LENGTH(topicHeader), &buffer[0]);

        // make sure we are still connected
        while (!mqtt->connected()){ mqtt->connect(clientID); delay(10); };

        Serial.println("Topic header:");
        Serial.println(topicHeader);

        Serial.println("Data:");
        for(size_t i = 0; i < buffer_length-MAC_NUM_BYTES; i++){
            Serial.print(buffer[MAC_NUM_BYTES+i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.flush();
        delay(1000);

        // send the MQTT data
        mqtt->publish(topicHeader, &buffer[MAC_NUM_BYTES], buffer_length-MAC_NUM_BYTES, false);

        if(espClient != nullptr){
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

void Gateway::createTopic(char* buffer, size_t max_len, uint8_t* nodeMAC){
    // structure: `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`
    
    size_t len = 0;

    strncpy(buffer, topic, max_len);
    len += strlen(topic);

    buffer[len] = '/';
    len++;

    // copy the MAC address of the gateway
    for(int i = 0; i < MAC_NUM_BYTES; i++){
        const char *fmt = "%02X";
        len += snprintf(&buffer[len], max_len-len, fmt, radioModule->getMACAddress()[i]);

        if(i != (MAC_NUM_BYTES-1)){
            buffer[len] = ':';
            len++;
        }
    }

    buffer[len] = '/';
    len++;

    // copy the MAC address of the node
    for(int i = 0; i < MAC_NUM_BYTES; i++){
        const char *fmt = "%02X";
        len += snprintf(&buffer[len], max_len-len, fmt, nodeMAC[i]);

        if(i != (MAC_NUM_BYTES-1)){
            buffer[len] = ':';
            len++;
        }
    }
}


void Gateway::createUpdateMessage(SensorNode_t& node, LoRaMessage& message){
    uint8_t data[27];
    uint32_t time;
    
    // copy MAC address
    std::memcpy(&data[0], node.macAddresss, MAC_NUM_BYTES);

    // copy command code
    data[6] = CommunicationCommandToInt(CommunicationCommand::TIME_CONFIG);

    // copy the current time
    time = rtc->read_time_epoch();
    std::memcpy(&data[7], &time, 4);

    // copy the sample time
    time = sampleTime;
    std::memcpy(&data[11], &time, 4);

    // copy the communication time
    time = node.nextCommunicationTime;
    std::memcpy(&data[15], &time, 4);

    // copy the sample period
    time = sampleInterval;
    std::memcpy(&data[19], &time, 4);

    // copy the communication period
    time = communicationInterval;
    std::memcpy(&data[23], &time, 4);

    message.setData(data, ARRAY_LENGTH(data));
}

void Gateway::createReadoutMessage(SensorNode_t& node, LoRaMessage& message){
    uint8_t data[7];
    
    // copy MAC address
    std::memcpy(&data[0], node.macAddresss, MAC_NUM_BYTES);

    // set the command
    data[6] = CommunicationCommandToInt(CommunicationCommand::REQUEST_MEASUREMENT_DATA);

    message.setData(data, ARRAY_LENGTH(data));
}



void Gateway::createAckMessage(LoRaMessage& original, LoRaMessage& reply){
    uint8_t buffer[7];

    // copy MAC address
    memcpy(buffer, original.getData(), 6);

    // indicate this is an ACK message
    buffer[6] = CommunicationCommandToInt(CommunicationCommand::ACK_DATA);

    // set correct data
    reply.setData(buffer, 7);
}

void Gateway::createDiscoveryMessage(LoRaMessage& m){
    uint8_t buffer[7];

    memcpy(buffer, getMacAddress(), 6);

    buffer[6] = CommunicationCommandToInt(CommunicationCommand::HELLO);

    m.setData(buffer, 7);    
}



uint32_t Gateway::getWiFiTime(void) {
    WiFiUDP ntpUDP;

    //NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
    NTPClient timeClient(ntpUDP, "be.pool.ntp.org", 3600, 60000);

    timeClient.begin();

    Serial.print("Fetching UTP time");

    while(!timeClient.update()){
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

uint32_t Gateway::getGSMTime(){
    return uplinkModule->get_rtc_time();
}

SensorNode_t* Gateway::addNewNode(LoRaMessage& m){
    SensorNode_t* node = nullptr;

    uint8_t* macAddress = m.getData();
    size_t j;
    bool newNode = true;

    Serial.print("Adding node: ");
    for(j = 0; j < 6; j++){
        Serial.print(macAddress[j], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // check if node in node list
    for(j = 0; j < nSensorNodes; j++){
        if(arrayCompare(macAddress, sensorNodes[j].macAddresss, 6)){
            newNode = false;
            node = &sensorNodes[j];
        }
    }

    if(nSensorNodes == ARRAY_LENGTH(sensorNodes)){
        newNode  = false;
    }

    // add new nodes to the node list
    if(newNode){
        auto time = rtc->read_time_epoch();
        Serial.println("Adding new node.");

        SensorNodeInit(&sensorNodes[nSensorNodes], macAddress);
        node = &sensorNodes[nSensorNodes];
        nSensorNodes++;

        node->lastCommunicationTime = time;
        node->nextCommunicationTime = readoutTime+nSensorNodes*communicationSpacing;

        Serial.print("Added node: ");
        for(j = 0; j < 6; j++){
            Serial.print(node->macAddresss[j], HEX);
            Serial.print(" ");
        }

        // write node to file
        nodeData = SPIFFS.open(nodeDataFN, FILE_WRITE);
        nodeData.write(nSensorNodes);
        for(j = 0; j < nSensorNodes; j++){
            nodeData.write((uint8_t*)  &sensorNodes[j], sizeof(SensorNode_t));
        }
        nodeData.close();
    }



    // the maximum amount of sensors is connected, stop discovery
    if(nSensorNodes == ARRAY_LENGTH(sensorNodes)){
        Serial.println("Maximum number of sensor nodes reached.");
    }

    return node;
}

void Gateway::storeSensorData(LoRaMessage& m){
    size_t length = 0;

    uint8_t* macAddress = m.getData();
    length = 7; // skip MAC and command byte

    uint8_t nValues;

    Serial.print("N bytes written: ");
    Serial.println(nBytesWritten);
    Serial.print("N bytes read: ");
    Serial.println(nBytesRead);

    if(openSensorDataFile()){
        while(length < m.getLength()){
            // write the MAC address
            nBytesWritten += sensorData.write(macAddress, MAC_NUM_BYTES);

            // write timestamp and number of values
            nBytesWritten += sensorData.write(&m.getData()[length], sizeof(uint32_t) + 1);
            length += sizeof(uint32_t) + 1;

            nValues = m.getData()[length-1];

            Serial.print("N values received: ");
            Serial.println(nValues);

            nBytesWritten += sensorData.write(&m.getData()[length], nValues*(sizeof(float) + 1));
            length += nValues*(sizeof(float) + 1);
        }

        Serial.print("N bytes written: ");
        Serial.println(nBytesWritten);

        sensorData.close();
    } else {
        Serial.println("Unable to open file.");
    }
}


bool openSensorDataFile(void){
    if(SPIFFS.exists(sensorDataFN)){
        sensorData = SPIFFS.open(sensorDataFN, FILE_APPEND);
    } else {
        sensorData = SPIFFS.open(sensorDataFN, FILE_WRITE);
    }

    if(sensorData){
        return true;
    } else {
        Serial.println("Unable to open data file.");
        return false;
    }
}

bool Gateway::checkNodeSource(SensorNode_t& node, LoRaMessage& message, CommunicationCommand cmd){
    if(!arrayCompare(node.macAddresss, message.getData(), MAC_NUM_BYTES)){
        return false;
    }
    if(message.getData()[6] != CommunicationCommandToInt(cmd)){
        return false;
    }
    return true;
}

File openDataFile(void){
    return SPIFFS.open(sensorDataFN, FILE_READ);
}