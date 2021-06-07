#include <Arduino.h>

#include "Radio.h"
#include "utilities.h"
#include "PCF2129_RTC.h"
#include "pins.h"
#include <algorithm>
#include "SPIFFS.h"

#define SS_PIN 27       // Slave select pin
#define RST_PIN 32      // Reset pin
#define DIO0_PIN 13     // DIO0 pin
#define DIO1_PIN 14     // This pin is not connected in this board version
#define TX_SWITCH 12 
#define RX_SWITCH 26

PCF2129_RTC rtc(RTC_INT_PIN, RTC_ADDRESS);
RadioModule radio(&rtc, SS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, TX_SWITCH, RX_SWITCH);

RTC_DATA_ATTR bool firstBoot = true;

// keep communication state in persistent memory
RTC_DATA_ATTR CommunicationState state = CommunicationState::SEARCHING_GATEWAY;
RTC_DATA_ATTR uint16_t n_errors = 0;

constexpr uint8_t sda_pin = 21, scl_pin = 22;

const char measurementDataFN[] = "/data.dat";

File measurementData;

const uint32_t gatewaySearchWindow = 120UL*1000UL;
const uint32_t gatewayCommunicationInterval = 10000UL;
const uint32_t sleepThreshold = 5; // seconds
const uint32_t wakeupTime = 3;

RTC_DATA_ATTR uint32_t next_sample_time;
RTC_DATA_ATTR uint32_t next_communication_time;
RTC_DATA_ATTR uint32_t sample_interval;
RTC_DATA_ATTR uint32_t communication_interval;
float sleepTime;
const float maxSleepTime = 60*60; // sleep at most one minute

RTC_DATA_ATTR size_t nBytesWritten;
RTC_DATA_ATTR size_t nBytesRead;

size_t measurementDataSize = 0;

RTC_DATA_ATTR uint8_t gatewayMAC[6];

void openDataFileWriteMode(void){
    // open the file with the sensor data
    if(SPIFFS.exists(measurementDataFN)){
        Serial.println("Opening existing file.");
        measurementData = SPIFFS.open(measurementDataFN, FILE_APPEND);
        measurementDataSize = measurementData.size();
    } else {
        Serial.println("Creating new file.");
        measurementData = SPIFFS.open(measurementDataFN, FILE_WRITE);
        measurementDataSize = 0;
    }
}

LoRaMessage message;
bool status;

void setup() {
    // Forcing GPIO16 ON to put power on VPP
    // The hold on the GPIO first needs to be disabled because after deep sleep 
    // the GPIO is low and still locked from the previous boot.
    gpio_hold_dis((gpio_num_t) 16);
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);
    gpio_hold_en((gpio_num_t) 16);

    delay(1);

    Serial.begin(115200);
    Wire.begin(sda_pin, scl_pin, 100e3);

    // mount file system
    if (!SPIFFS.begin(true)) {
        Serial.println("Mounting SPIFFS failed");
        SPIFFS.format();
        ESP.restart(); 
    }

    openDataFileWriteMode();

    if(!measurementData){
        Serial.println("There was an error with the data file.");
        SPIFFS.format();
        ESP.restart();        
    }
    

    while(rtc.begin() != I2C_ERROR_OK){    
        Serial.println("Connecting to RTC...");
        Serial.print("Error code: ");
        Serial.println(rtc.begin());

        delay(100);
    }

    radio.init();
    
    if (firstBoot){
        Serial.print("First boot. Setting time...");

        // Writing the initial time
        rtc.write_time_epoch(1546300800);
        Serial.println(" done.");

        Serial.print("MAC address: ");
        for(int i = 0; i < 6; i++){
            Serial.print(radio.getMACAddress()[i]);
            Serial.print(" ");
        }
        Serial.println();

        for(int i = 0; i < ARRAY_LENGTH(gatewayMAC); i++){
            gatewayMAC[i] = 0;
        }

        firstBoot = false;
        nBytesWritten = measurementDataSize;
        nBytesRead = measurementDataSize;

        Serial.print("nBytesWritten: ");
        Serial.println(nBytesWritten);

        Serial.print("nBytesRead: ");
        Serial.println(nBytesRead);

        sample_interval = 10*60;
        communication_interval = 20*60;

        Serial.println("Testing sensor readout.");

        initialiseSensors();

        startMeasurements();

        delay(100);

        readoutToBuffer(message.getData(), MAX_MESSAGE_LENGTH, rtc.read_time_epoch());

        stopMeasurements();
    } else {
        initialiseSensors();

        Serial.print("Wakeup cause: ");
        Serial.println(esp_sleep_get_wakeup_cause());
    }

    measurementData.close();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

void loop() {
    status = false;

    switch (state){
        case CommunicationState::READ_SENSOR_DATA: {
            Serial.println("State: Reading all sensors.");
            startMeasurements();

            // if there is not enough space left, delete data
            if((SPIFFS.usedBytes() > 0) && (SPIFFS.usedBytes() > (SPIFFS.totalBytes()-0x100))){
                Serial.println("Removing all data.");
                SPIFFS.remove(measurementDataFN);

                nBytesWritten = 0;
                nBytesRead = 0;
            }

            openDataFileWriteMode();

            // wait until the timepoint is correct
            if(rtc.read_time_epoch() <  next_sample_time){
                int64_t sleep_time;
                sleep_time = next_sample_time - rtc.read_time_epoch();
                sleep_time *= 500000;
                do{
                    Serial.print("Light sleep for ");
                    Serial.println((uint32_t) sleep_time);
                    esp_sleep_enable_timer_wakeup(sleep_time); // sleep for 0.5 seconds
                    esp_light_sleep_start();
                    sleep_time = 500000;
                } while(rtc.read_time_epoch() <  next_sample_time );
            }

            size_t length = readoutToBuffer(message.getData(), MAX_MESSAGE_LENGTH, rtc.read_time_epoch());
            stopMeasurements();

            // store data
            Serial.print("Wrote from ");
            Serial.print(nBytesWritten);
            nBytesWritten += measurementData.write(message.getData(), length);
            Serial.print(" to ");
            Serial.println(nBytesWritten);

            Serial.print("Wrote the following data: ");
            for(size_t i = 0; i < length; i++){
                Serial.print(message.getData()[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            
            // set next measurement time
            next_sample_time += sample_interval;

            measurementData.close();
            
            state = CommunicationState::SLEEP;
            break;
        }
        case CommunicationState::ERROR:
            Serial.println("State: CommunicationState::ERROR");
            Serial.flush();
            
            deepSleep(60);
            break;
        case CommunicationState::SEARCHING_GATEWAY:
            Serial.println("State: Searching for gateway.");

            status = radio.receiveAnyMessage(gatewaySearchWindow, message);
            #ifdef __DEBUG_COMM__
            status = true; message.setData((uint8_t*) "HELLOO\x04", 7);
            #endif
            if(status && checkCommand(message, CommunicationCommand::HELLO)){
                uint8_t data[7];

                // we send our MAC address
                Serial.println("Received hello message");

                // store the MAC of the gateway
                memcpy(gatewayMAC, message.getData(), ARRAY_LENGTH(gatewayMAC));

                // send our MAC address
                memcpy(data, radio.getMACAddress(), 6);
                data[6] = CommunicationCommandToInt(CommunicationCommand::HELLO_REPLY);
                message.setData(data, ARRAY_LENGTH(data));
                radio.sendMessage(message);

                state = CommunicationState::GET_SAMPLE_CONFIG;
            }

            break;
        case CommunicationState::GET_SAMPLE_CONFIG:{
            Serial.println("Waiting for time configuration.");
            status = radio.receiveSpecificMessage(gatewayCommunicationInterval, message, CommunicationCommand::TIME_CONFIG);
            #ifdef __DEBUG_COMM__
            uint8_t data[] = {180, 230, 45, 221, 58, 89, //180, 230, 45, 220, 28, 45, // 
                              0x01,  
                              0x5f, 0x11, 0x61, 0x19, 
                              0x5f, 0x11, 0x62, 0x23, 
                              0x5f, 0x11, 0x62, 0x28};
            status = true; message.setData(data, ARRAY_LENGTH(data));
            #endif

            if(status){
                Serial.println("Received time configuration.");
                if(readTimeData(message)){
                    state = CommunicationState::SLEEP;
                } else {
                    state = CommunicationState::SEARCHING_GATEWAY;
                }
            } else {
                state = CommunicationState::SEARCHING_GATEWAY;
            }
        }
            
            break;

        case CommunicationState::UPLOAD_SENSOR_DATA:
            // wait for the command from the gateway
            status = radio.receiveSpecificMessage(gatewayCommunicationInterval, message, CommunicationCommand::REQUEST_MEASUREMENT_DATA);

            if(status){

                uint8_t buffer[256];
                size_t readLength = ARRAY_LENGTH(buffer) - 7;

                Serial.println("Uploading data to gateway.");

                // prepare fixed part of header
                memcpy(buffer, radio.getMACAddress(), 6);
                buffer[6] = CommunicationCommandToInt(CommunicationCommand::MEASUREMENT_DATA);

                // move to last position read
                measurementData = SPIFFS.open(measurementDataFN, FILE_READ);
                measurementData.seek(nBytesRead);

                // read at most 10 samples
                readLength = min(readLength, nBytesWritten - nBytesRead);
                readLength = readMeasurementData(&buffer[7], readLength, measurementData);

                measurementData.close();
                
                // prepare data transmission
                message.setData(buffer, readLength+7);

                radio.sendMessage(message);

                // check for ACK
                status = radio.receiveSpecificMessage(gatewayCommunicationInterval, message, CommunicationCommand::TIME_CONFIG);

                // update variables if we received an ACK message from the gateway
                if(status){
                    nBytesRead += readLength;

                    Serial.println("Received ACK and timing update.");

                    readTimeData(message);

                    buffer[6] = CommunicationCommandToInt(CommunicationCommand::ACK_DATA);
                    message.setData(buffer, 7);
                    radio.sendMessage(message);

                    if(nBytesRead == nBytesWritten){
                        SPIFFS.remove(measurementDataFN);
                        
                        nBytesRead = 0;
                        nBytesWritten = 0;

                        // create file
                        openDataFileWriteMode();
                        measurementData.close();
                    }
                } else {
                    n_errors++;

                    next_communication_time += communication_interval;
                }
                
            } else {
                Serial.println("No reply received.");
                n_errors++;

                Serial.print("Communication time from ");
                Serial.print(next_communication_time);
                next_communication_time += communication_interval;
                Serial.print(" to ");
                Serial.println(next_communication_time);
            }
            
            state = CommunicationState::SLEEP;

            break;

        case CommunicationState::SLEEP: {
            // TODO: determine sleep time and determine the next state
            Serial.println("Sleep state");

            uint32_t now = rtc.read_time_epoch();

            // check if the next sample time is close at hand
            if((next_sample_time - sleepThreshold) < now){
                state = CommunicationState::READ_SENSOR_DATA;

                Serial.println("Directly switching to sensor read state.");
                Serial.flush();
                break;
            }
            
            // check if the next communication time is close at hand
            if((next_communication_time - sleepThreshold) < now){
                state = CommunicationState::UPLOAD_SENSOR_DATA;

                Serial.print("left value: ");
                Serial.println(next_communication_time - sleepThreshold);
                Serial.print("Right value: ");
                Serial.println(now);

                Serial.println("Directly switching to communication state.");
                Serial.flush();
                break;
            }

            // check next wakeup event cause: sampling or communication
            if(next_sample_time <= next_communication_time){
                state = CommunicationState::READ_SENSOR_DATA;
                sleepTime = ((next_sample_time - wakeupTime) - now);
            } else {
                state = CommunicationState::UPLOAD_SENSOR_DATA;
                sleepTime = ((next_communication_time - wakeupTime) - now);
            }

            if(sleepTime > maxSleepTime){
                Serial.print("Max sleep time exceeded. Limiting sleep time from ");
                Serial.print(sleepTime);
                Serial.print(" to ");
                Serial.print(maxSleepTime);
                Serial.println("s.");
                sleepTime = maxSleepTime;
                state = CommunicationState::SLEEP;
            }

            Serial.print("Sleeping for ");
            Serial.print(sleepTime);
            Serial.println(" seconds.");
            Serial.flush();
            delay(1000);
            Serial.flush();

            if(sleepTime > 0){
                digitalWrite(16, LOW);
                deepSleep(sleepTime);
            }

            break;
        }

        case CommunicationState::UART_READOUT:
            Serial.println("Data in memory:");
            measurementData = SPIFFS.open(measurementDataFN, FILE_READ);
            if(measurementData){
                size_t n_bytes_read = 0;
                size_t n_bytes_read_old = 0;
                size_t read_length;
                size_t i;

                uint8_t data_buffer[10];
                while(n_bytes_read < measurementData.size()){
                    n_bytes_read_old = n_bytes_read;
                    n_bytes_read = min(n_bytes_read + 10, measurementData.size());
                    read_length = n_bytes_read-n_bytes_read_old;

                    measurementData.read(data_buffer, read_length);

                    for(i = 0; i < read_length; i++){
                        Serial.print(data_buffer[i], HEX);
                    }
                    Serial.println();
                }

                measurementData.close();
            } else {
                Serial.println("There was an error.");
            }
            state = CommunicationState::SLEEP;
            break;

    }
        Serial.println("loop");
}