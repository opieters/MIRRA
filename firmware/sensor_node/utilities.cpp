#include "Arduino.h"
#include "utilities.h"
#include "Sensor.h"
#include "TempRHSensor.h"
#include "SoilMoistureSensor.h"
#include "LightSensor.h"
#include "SoilTempSensor.h"
#include "PCF2129_RTC.h"
#include "Radio.h"
#include "pins.h"
#include "ESPCam.h"

// The time between gateway activations / sample time
const uint32_t gateway_receive_window = 20 * 60; // 20 minutes in seconds

// This is the time at which the gateway will be on next time (epoch reference).
// This time is unknown before any valid communication
RTC_DATA_ATTR uint32_t gateway_next_on;

extern PCF2129_RTC rtc;
extern RadioModule radio;

float sleep_time;
extern uint32_t next_sample_time, next_communication_time, sample_interval, communication_interval;

size_t readoutToBuffer(uint8_t *buffer, size_t max_length, uint32_t ctime, Sensor **sensorArray, uint8_t n_sensors)
{
    size_t nBytesWritten = 0, nSensorValuesLocation;
    uint8_t nSensorValues = 0;

    Serial.println("Filling payload");

    // buffer = memory element of certain size
    memcpy(buffer, &ctime, sizeof(ctime));
    // to keep up with how far in buffer data has been written (necessary to know where to continue writing after first sampling)
    nBytesWritten += sizeof(ctime);

    // Now we add a byte that indicates the size of the payload
    // The payload size is the size of all the sensor data (a sensor is always a float number + 1 byte key)
    nSensorValuesLocation = nBytesWritten;
    nBytesWritten++;

    // Let's read all the sensors and add their data into the packet_buf array

    float sensorReadout[2];
    uint8_t nValues;

    for (size_t i = 0; i < n_sensors; ++i)
    {
        Serial.print("Sensor ");

        Serial.print(sensorArray[i]->getID());
        Serial.print(" (");
        Serial.print(i);
        Serial.print("): ");

        // read sensor data
        nValues = sensorArray[i]->read_measurement(sensorReadout, ARRAY_LENGTH(sensorReadout));
        nSensorValues += nValues;

        // copy sensor data to the buffer
        for (size_t j = 0; j < nValues; j++)
        {
            // read sensor id and copy to the buffer
            buffer[nBytesWritten] = sensorArray[i]->getID() + j;
            nBytesWritten++;

            // copies value from one buffer to another (memorycopy)
            memcpy(&buffer[nBytesWritten], &sensorReadout[j], sizeof(sensorReadout[j]));
            nBytesWritten += sizeof(sensorReadout[j]);

            Serial.print(sensorReadout[j]);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("Number of sensor values: ");
        Serial.println(nSensorValues);

        // write correct number of sensor values read to buffer
        buffer[nSensorValuesLocation] = nSensorValues;
    }

    return nBytesWritten;
}

void deepSleep(float sleep_time)
{

    radio.sleep();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep sleep
    if (sleep_time < 10)
    {
        Serial.println("Light sleep.");
        Serial.flush();
        // We use the internal timer
        esp_sleep_enable_timer_wakeup(SECONDS_TO_US(sleep_time));

        digitalWrite(16, LOW);
        gpio_hold_en((gpio_num_t)16);

        esp_deep_sleep_start();
    }
    else
    {
        // We use the external RTC
        Serial.println("Deep sleep");
        Serial.flush();
        uint32_t now = rtc.read_time_epoch();
        rtc.write_alarm_epoch(now + (uint32_t)round(sleep_time));
        rtc.enable_alarm();

        digitalWrite(16, LOW);
        gpio_hold_en((gpio_num_t)16);

        esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc.getIntPin(), 0);
        esp_sleep_enable_ext1_wakeup((gpio_num_t)(1 << 0), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
        esp_deep_sleep_start();
    }
}

bool checkCommand(LoRaMessage &message, CommunicationCommand command)
{
    if (message.getLength() < 7)
    {
        return false;
    }

    uint8_t cmd = CommunicationCommandToInt(command);

    return message.getData()[6] == cmd;
}

// sensor node receives current time from gateway
bool readTimeData(LoRaMessage &message, Sensor **sensors, size_t n_sensors, bool onlyEpoch)
{
    //next_communication_time = rtc.read_time_epoch() + 120;
    //return true;

    uint32_t time;
    size_t i;

    if (message.getLength() != 27)
    {
        return false;
    }

    // timepoints are determined by gateway and communicated to nodes
    // between [] is position from which to start reading to save correct timepoints into variables in the sensor node memory (eg. next_sample_time)
    memcpy(&time, &message.getData()[7], sizeof(time));
    rtc.write_time_epoch(time);
    if (!onlyEpoch)
    {
        memcpy(&next_sample_time, &message.getData()[7 + 4], sizeof(next_sample_time));
        memcpy(&next_communication_time, &message.getData()[7 + 4 + 4], sizeof(next_communication_time));
        memcpy(&sample_interval, &message.getData()[7 + 4 + 4 + 4], sizeof(sample_interval));
        memcpy(&communication_interval, &message.getData()[7 + 4 + 4 + 4 + 4], sizeof(communication_interval));

        for (i = 0; i < n_sensors; i++)
        {
            sensors[i]->set_sample_interval(sample_interval);
        }
    }
    return true;
}

size_t readMeasurementData(uint8_t *buffer, const size_t max_length, File &file)
{
    size_t length = 0;   // current length of buffer
    size_t prev_idx = 0; // last full data entry of sample event

    // read timestamp and data length: to make sure that full sample (i.e. data from all sensors) is transfered
    while (length < max_length)
    {
        // uint32_t = length of time (4 bytes)
        if ((length + sizeof(uint32_t) + 1) >= max_length)
        {
            return prev_idx;
        }
        file.read(&buffer[length], sizeof(uint32_t) + 1);
        length += sizeof(uint32_t) + 1;

        size_t nValues = buffer[length - 1];

        if ((nValues * (sizeof(float) + 1)) >= max_length)
        {
            return prev_idx;
        }

        // read data length into buffer
        file.read(&buffer[length], nValues * (sizeof(float) + 1));
        length += nValues * (sizeof(float) + 1);

        prev_idx = length;
    }

    return prev_idx;
}