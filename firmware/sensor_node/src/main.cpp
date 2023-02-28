#include <Arduino.h>

#include "Radio.h"
#include "utilities.h"
#include "PCF2129_RTC.h"
#include "pins.h"
#include <algorithm>
#include "SPIFFS.h"
#include "Sensor.h"
#include "TempRHSensor.h"
// #include "SoilMoistureSensor.h"
#include "LightSensor.h"
#include "SoilTempSensor.h"
#include "ESPCam.h"
#include <math.h>
#include <Software/OWI.h>
#include "ESPCamOW.h"
#include <HardwareSerial.h>
#include <ESPCamUART.h>
#include <driver/adc.h>

#define SS_PIN 27   // Slave select pin
#define RST_PIN 26  // Reset pin
#define DIO0_PIN 34 // DIO0 pin IRQ
#define DIO1_PIN 14 // This pin is not connected in this board version
#define TX_SWITCH 13
#define RX_SWITCH 25
// for debugging purposes
#define __DEBUG__ // comment out when not debugging

PCF2129_RTC rtc(RTC_INT_PIN, RTC_ADDRESS);
RadioModule radio(&rtc, SS_PIN, RST_PIN, DIO0_PIN, DIO1_PIN, TX_SWITCH, RX_SWITCH);

// RTC_DATA_ATTR geeft aan dat die variabele in speciaal deel van geheugen worden opgeslaan (want bij deepsleep wordt alles vergeten)
RTC_DATA_ATTR bool firstBoot = true;

// keep communication state in persistent memory
RTC_DATA_ATTR CommunicationState state = CommunicationState::SEARCHING_GATEWAY, prev_state = CommunicationState::SLEEP;
RTC_DATA_ATTR uint16_t n_errors = 0;

constexpr uint8_t sda_pin = 21, scl_pin = 22;

const char measurementDataFN[] = "/data.dat";

File measurementData;

const uint32_t gatewaySearchWindow = 120UL * 1000UL;
const uint32_t gatewayCommunicationInterval = 10000UL;

RTC_DATA_ATTR uint32_t next_sample_time;
RTC_DATA_ATTR uint32_t next_communication_time;
RTC_DATA_ATTR uint32_t sample_interval;
RTC_DATA_ATTR uint32_t communication_interval;
float sleepTime;
const float maxSleepTime = 12 * 60 * 60; // sleep at most half a day

RTC_DATA_ATTR size_t nBytesWritten;
RTC_DATA_ATTR size_t nBytesRead;

size_t measurementDataSize = 0;

RTC_DATA_ATTR uint8_t gatewayMAC[6];

LoRaMessage message;
bool status;

// TODO
const uint8_t oneWirePin = 17;
// const uint8_t uartWirePin = 17;
// const uint8_t soilMoisturePin = 39;

// Software::OWI owi(2);
HardwareSerial serial2(2);

// SoilMoistureSensor soilMoisture(soilMoisturePin);
TempRHSensor airTempRHSensor;
LightSensor lightSensor;
SoilTemperatureSensor soilTempSensor(oneWirePin, 0);

ESPCamUART espCamera1(&serial2, GPIO_NUM_2);

Sensor *sensors[] = {
    &airTempRHSensor,
    &soilTempSensor,
    &lightSensor,
    &espCamera1,
    //&espCamera2,
};

const size_t n_sensors = ARRAY_LENGTH(sensors);

RTC_DATA_ATTR Sensor *sampleSensors[n_sensors] = {
    &airTempRHSensor,
    &soilTempSensor,
    &lightSensor,
    &espCamera1,
};
RTC_DATA_ATTR uint8_t n_sampleSensors = 4;

size_t i;

void openDataFileWriteMode(void)
{
    // open the file with the sensor data
    if (SPIFFS.exists(measurementDataFN))
    {
#ifdef __DEBUG__
        Serial.println("Opening existing file.");
#endif
        measurementData = SPIFFS.open(measurementDataFN, FILE_APPEND);
        measurementDataSize = measurementData.size();
    }
    else
    {
#ifdef __DEBUG__
        Serial.println("Creating new file.");
#endif
        measurementData = SPIFFS.open(measurementDataFN, FILE_WRITE);
        measurementDataSize = 0;
    }
}

void initialiseAllSensors(void)
{
    size_t i;

#ifdef __DEBUG__
    Serial.println("Initialising ");
    Serial.print(n_sensors);
    Serial.println(" sensors");
#endif

    for (i = 0; i < n_sensors; i++)
    {
        sensors[i]->setup();
    }
}

void stopMeasurements(void)
{
    size_t i;

#ifdef __DEBUG__
    Serial.println("Stop measurements.");
#endif

    for (i = 0; i < n_sensors; i++)
    {
        sensors[i]->stop_measurement();
    }
}

bool dump_uart_log = false;
void IRAM_ATTR gpio_0_isr_callback()
{
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
    {
        dump_uart_log = true;
        state = CommunicationState::SLEEP;
    }
}

void setup()
{
    // Forcing GPIO16 ON to put power on VPP
    // The hold on the GPIO first needs to be disabled because after deep sleep
    // the GPIO is low and still locked from the previous boot.
    // Needed to enable SDA qnd SCL pull-up voltages for RTC communication
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);
    gpio_hold_dis((gpio_num_t)16);

    // ss.enableIntTx(false);
    // ss.enableRx(false);

    Serial.begin(115200);
    Wire.begin(sda_pin, scl_pin, 100e3); // starts i2c

    attachInterrupt(0, gpio_0_isr_callback, FALLING);

    // mount file system
    if (!SPIFFS.begin(true))
    {
#ifdef __DEBUG__
        Serial.println("Mounting SPIFFS failed");
#endif
        // remove all data from file system is there was a critical error
        SPIFFS.format();
        ESP.restart();
    }
    

    // try to open data file
    openDataFileWriteMode();

    if (!measurementData)
    {
#ifdef __DEBUG__
        Serial.println("There was an error with the data file.");
#endif
        SPIFFS.format();
        ESP.restart();
    }

    // connect with RTC
    while (rtc.begin())
    {
#ifdef __DEBUG__
        Serial.println("Connecting to RTC...");
        Serial.print("Error code: ");
        Serial.println(rtc.begin());
#endif

        delay(100);
    }

    // start radio module
    radio.init();

    // in case of the very first boot event, we need to set the clock, find the gateway and the correct time
    // sensor connections are also checked
    if (firstBoot)
    {
#ifdef __DEBUG__
        Serial.print("First boot. Setting time...");
#endif

        // Writing the initial time: dummy time (is changed after communication with gateway)
        rtc.write_time_epoch(1560210042);
#ifdef __DEBUG__
        Serial.println(" done.");
#endif

        // MAC address is used for identification: unique MAC address per node used as ID in datafile
        Serial.print("MAC address: ");
        for (int i = 0; i < 6; i++)
        {
            Serial.print(radio.getMACAddress()[i]);
            Serial.print(" ");
        }
        Serial.println();

        for (int i = 0; i < ARRAY_LENGTH(gatewayMAC); i++)
        {
            gatewayMAC[i] = 0; // gateway MAC is initialised to 0
        }

        firstBoot = false;

        // if file exists, we append to it. It is assumed all data is sent after a full restart-event
        // i.e. when the state of the RTC_DATA_ATTR variables is also lost
        nBytesWritten = measurementDataSize;
        nBytesRead = measurementDataSize;

#ifdef __DEBUG__
        Serial.print("nBytesWritten: ");
        Serial.println(nBytesWritten);

        Serial.print("nBytesRead: ");
        Serial.println(nBytesRead);
#endif

        // default sample interval: 10 minutes (read out data from sensors)
        sample_interval = 20 * 60;
        // default communication interval: 20 minutes (communication with gateway)
        // communication always occurs between sample timepoint, never at the same time!
        communication_interval = 60 * 60;

#ifdef __DEBUG__
        Serial.println("Testing sensor readout.");
#endif
        initialiseAllSensors();

        for (i = 0; i < n_sensors; i++)
        {
            sensors[i]->start_measurement();
        }

        delay(100);

        // reading data from sensors: assembled in LoRa message format
        readoutToBuffer(message.getData(), MAX_MESSAGE_LENGTH, rtc.read_time_epoch(), sensors, n_sensors);

        stopMeasurements();

        delay(10000);
    }
    else
    {
#ifdef __DEBUG__
        Serial.print("Wakeup cause: ");
#endif
        Serial.println(esp_sleep_get_wakeup_cause());
    }

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
    {
        gpio_0_isr_callback();
    }

    measurementData.close();

    // eeded because this variable's state is not retained in deep sleep
    for (i = 0; i < n_sensors; i++)
    {
        sensors[i]->set_sample_interval(sample_interval);
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

void loop()
{
    status = false;

    // the main event-loop is implemented as a state-machine
    // the default state is the SLEEP state, which determines the next state

    if (dump_uart_log)
    {
        prev_state = state;
        state = CommunicationState::UART_READOUT;
        dump_uart_log = false;
    }

    switch (state)
    {
    case CommunicationState::PREPARE_READOUT:
#ifdef __DEBUG__
        Serial.println("Initialising ");
        Serial.print(n_sampleSensors);
        Serial.println(" sensors");
#endif

        for (i = 0; i < n_sampleSensors; i++)
        {
            sampleSensors[i]->setup();
        }

        state = CommunicationState::READ_SENSOR_DATA;
        break;
    // sensor readout state
    // note that not all sensors are read out at every timestep
    case CommunicationState::READ_SENSOR_DATA:
    {
#ifdef __DEBUG__
        Serial.println("State: Reading all sensors.");

        Serial.println("Starting measurements");
#endif

        for (i = 0; i < n_sampleSensors; i++)
        {
            sampleSensors[i]->start_measurement();
        }

        // if there is not enough space left, stop measuring until after the next communication event
        if ((SPIFFS.usedBytes() > 0) && (SPIFFS.usedBytes() > (SPIFFS.totalBytes() - 0x100)))
        {
            state = CommunicationState::SLEEP;
            next_sample_time += communication_interval;
        }

        // open file to store the measurement data
        openDataFileWriteMode();

        // wait until the timepoint is correct
        // implemented with a 0.5s sleep delay because reading the RTC state too fast
        // can result into an errorous readout and thus cause the system to fail
        if (rtc.read_time_epoch() < next_sample_time)
        {
            int64_t sleep_time;
            sleep_time = next_sample_time - rtc.read_time_epoch();
            sleep_time *= 500000;
            do
            {
                Serial.print("Light sleep for ");
                Serial.println((uint32_t)sleep_time);
                esp_sleep_enable_timer_wakeup(sleep_time); // sleep for 0.5 seconds
                esp_light_sleep_start();
                sleep_time = 500000;
            } while (rtc.read_time_epoch() < next_sample_time);
        }

        // copy sensor data (and readout timepoint) into the buffer and stop the measurements
        size_t length = readoutToBuffer(message.getData(), MAX_MESSAGE_LENGTH, rtc.read_time_epoch(), sampleSensors, n_sampleSensors);
        stopMeasurements();

// store data
#ifdef __DEBUG__
        Serial.print("Wrote from ");
        Serial.print(nBytesWritten);
#endif
        // write buffer to file
        nBytesWritten += measurementData.write(message.getData(), length);
#ifdef __DEBUG__
        Serial.print(" to ");
        Serial.println(nBytesWritten);
#endif

#ifdef __DEBUG__
        Serial.print("Wrote the following data: ");
        for (size_t i = 0; i < length; i++)
        {
            Serial.print(message.getData()[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
#endif

        // update sensor list that must be read at next sample event

        // find closest sample event
        uint32_t interval = UINT_MAX;
        uint32_t sensor_interval;
        n_sampleSensors = 0;
        for (i = 0; i < n_sensors; i++)
        {
            Serial.print("Sensor ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(sensors[i]->adaptive_sample_interval_update(next_sample_time));
            sensor_interval = sensors[i]->adaptive_sample_interval_update(next_sample_time);
            if (sensor_interval < interval)
            {
                n_sampleSensors = 1;
                interval = sensor_interval;
                sampleSensors[0] = sensors[i];
            }
            else if (sensor_interval == interval)
            {
                sampleSensors[n_sampleSensors] = sensors[i];
                n_sampleSensors++;
            }
        }

        // set next measurement time
        next_sample_time += interval;

#ifdef __DEBUG__
        Serial.print("Time until next sample event: ");
        Serial.println(interval);
#endif

        measurementData.close();

        state = CommunicationState::SLEEP;
        break;
    }

    // a serious error occurred -> sleep for 60 seconds
    // currently, this state cannot be reached, but might
    // be useful for future error-recovery
    case CommunicationState::ERROR:
#ifdef __DEBUG__
        Serial.println("State: CommunicationState::ERROR");
        Serial.flush();
#endif

        deepSleep(60);
        break;

    // first boot -> look for gateway
    case CommunicationState::SEARCHING_GATEWAY:
    {
#ifdef __DEBUG__
        Serial.println("State: Searching for gateway.");
#endif

        uint8_t data[7];

// we send our MAC address
#ifdef __DEBUG__
        Serial.println("Sending MAC");
#endif

        // store the MAC of the gateway (in hello message from gateway)
        memcpy(gatewayMAC, message.getData(), ARRAY_LENGTH(gatewayMAC));

        // send our MAC address: from node to gateway
        memcpy(data, radio.getMACAddress(), 6);
        data[6] = CommunicationCommandToInt(CommunicationCommand::HELLO_REPLY);
        message.setData(data, ARRAY_LENGTH(data));
        radio.sendMessage(message);

        state = CommunicationState::GET_SAMPLE_CONFIG;
    }

    break;
    // after locating the gateway, we need to acquire the current time ans sample rate from the gateway
    case CommunicationState::GET_SAMPLE_CONFIG:
    {
#ifdef __DEBUG__
        Serial.println("Waiting for time configuration.");
#endif
        status = radio.receiveSpecificMessage(gatewayCommunicationInterval, message, CommunicationCommand::TIME_CONFIG);
#ifdef __DEBUG_COMM__
        uint8_t data[] = {180, 230, 45, 221, 58, 89, // 180, 230, 45, 220, 28, 45, //
                          0x01,
                          0x5f, 0x11, 0x61, 0x19,
                          0x5f, 0x11, 0x62, 0x23,
                          0x5f, 0x11, 0x62, 0x28};
        status = true;
        message.setData(data, ARRAY_LENGTH(data));
#endif

        if (status)
        {
#ifdef __DEBUG__
            Serial.println("Received time configuration.");
#endif
            delay(300);
            if (readTimeData(message, sensors, n_sensors))
            {
                state = CommunicationState::SLEEP;
            }
            else
            {
                state = CommunicationState::SEARCHING_GATEWAY;
            }
            Serial.println(rtc.read_time_epoch());
        }
        else
        {
            state = CommunicationState::SEARCHING_GATEWAY;
        }
    }

    break;

    // send sensor data to the gateway
    case CommunicationState::UPLOAD_SENSOR_DATA:
    {
        // wait for the command from the gateway, the gateway always takes initiative!
        // gateway signals that it wants to receive message from specific node --> sensor node must be in this state to listen to this message
        // buffer to save data to before sending out
        uint8_t buffer[256];
        size_t readLength = ARRAY_LENGTH(buffer) - 7;

#ifdef __DEBUG__
        Serial.println("Uploading data to gateway.");
#endif

        // prepare fixed part of header
        memcpy(buffer, radio.getMACAddress(), 6);
        buffer[6] = CommunicationCommandToInt(CommunicationCommand::MEASUREMENT_DATA);

        // move to last position read
        measurementData = SPIFFS.open(measurementDataFN, FILE_READ);
        measurementData.seek(nBytesRead);

        // read at most 10 samples
        readLength = min(readLength, nBytesWritten - nBytesRead);
        // written as of position 7 (before: MAC address)
        readLength = readMeasurementData(&buffer[7], readLength, measurementData);

        measurementData.close();

        // prepare data transmission
        message.setData(buffer, readLength + 7);

        radio.sendMessage(message);

        // check for ACK (acknowledge): checking if gateway has received message
        status = radio.receiveSpecificMessage(gatewayCommunicationInterval, message, CommunicationCommand::TIME_CONFIG);

        // update variables if we received an ACK message from the gateway
        if (status)
        {
            nBytesRead += readLength;

#ifdef __DEBUG__
            Serial.println("Received ACK and timing update.");
#endif

            readTimeData(message, sensors, n_sensors, false);
            // another ACK from node to gateway --> gateway knows that node received message with next sample timepoint
            buffer[6] = CommunicationCommandToInt(CommunicationCommand::ACK_DATA);
            message.setData(buffer, 7);
            radio.sendMessage(message);

            // OPTIONAL: remove data from sensor node when sure that gateway has received data
            // not necessary because enough storage in node
            /*if(nBytesRead == nBytesWritten){
                SPIFFS.remove(measurementDataFN);

                nBytesRead = 0;
                nBytesWritten = 0;

                // create file
                openDataFileWriteMode();
                measurementData.close();
            }*/
        }
        else
        {
            n_errors++;

            next_communication_time += communication_interval;
        }

        state = CommunicationState::SLEEP;

        break;
    }
    case CommunicationState::SLEEP:
    {
// Determine sleep time and determine the next state
#ifdef __DEBUG__
        Serial.println("Sleep state");
#endif

        uint32_t now = rtc.read_time_epoch();

        // check if the next sample time is close at hand
        if ((next_sample_time) < now)
        {
            state = CommunicationState::PREPARE_READOUT;

#ifdef __DEBUG__
            Serial.println("Directly switching to sensor read state.");
            Serial.flush();
#endif
            break;
        }

        // check if the next communication time is close at hand
        if ((next_communication_time) < now)
        {
            state = CommunicationState::UPLOAD_SENSOR_DATA;

#ifdef __DEBUG__
            Serial.print("left value: ");
            Serial.println(next_communication_time);
            Serial.print("Right value: ");
            Serial.println(now);

            Serial.println("Directly switching to communication state.");
            Serial.flush();
#endif
            break;
        }

        // check next wakeup event cause: sampling or communication
        if (next_sample_time <= next_communication_time)
        {
            state = CommunicationState::PREPARE_READOUT;
            // wakeupTime = time necessary for start up
            sleepTime = ((next_sample_time) - now);
            Serial.println("Next event: sensor readout.");
            Serial.flush();
        }
        else
        {
            state = CommunicationState::UPLOAD_SENSOR_DATA;
            sleepTime = ((next_communication_time) - now);
            Serial.println("Next event: gateway comms.");
            Serial.flush();
        }

        if (sleepTime > maxSleepTime)
        {
#ifdef __DEBUG__
            Serial.print("Max sleep time exceeded. Limiting sleep time from ");
            Serial.print(sleepTime);
            Serial.print(" to ");
            Serial.print(maxSleepTime);
            Serial.println("s.");
#endif
            sleepTime = maxSleepTime;
            state = CommunicationState::SLEEP;
        }

#ifdef __DEBUG__
        Serial.print("Sleeping for ");
        Serial.print(sleepTime);
        Serial.println(" seconds.");
        Serial.flush();
        delay(1000);
        Serial.flush();
#endif

        if (sleepTime > 0)
        {

            // ! after every deep sleep: node returns to setup (only RTC_DATA_ATTR variables remain stored)
            deepSleep(sleepTime);
        }

        break;
    }
    // This state can be used to readout the sensor data in case there was a communication issue.
    // Currently, the software needs to be re-flashed to enter this state (forcing state to UART_READOUT around line 20). This should be
    // modified such that sening a special UART command can trigger a print to UART
    // TODO: improve this
    case CommunicationState::UART_READOUT:
#ifdef __DEBUG__
        Serial.println("Data in memory:");
#endif
        measurementData = SPIFFS.open(measurementDataFN, FILE_READ);
        if (measurementData)
        {
            size_t n_bytes_read = 0;
            size_t n_bytes_read_old = 0;
            size_t read_length;
            size_t i;

            uint8_t data_buffer[10];
            while (n_bytes_read < measurementData.size())
            {
                n_bytes_read_old = n_bytes_read;
                n_bytes_read = min(n_bytes_read + 10, measurementData.size());
                read_length = n_bytes_read - n_bytes_read_old;

                measurementData.read(data_buffer, read_length);

                char print_buffer[10];
                for (i = 0; i < read_length; i++)
                {
                    sprintf(print_buffer, "%02x", data_buffer[i]);
                    Serial.print(print_buffer);
                }
                Serial.println();
            }

            measurementData.close();
        }
        else
        {
#ifdef __DEBUG__
            Serial.println("There was an error.");
#endif
        }
        state = prev_state;
        break;
    }
#ifdef __DEBUG__
    Serial.println("loop");
#endif
}