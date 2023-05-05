#include "sensornode.h"


RTC_DATA_ATTR bool firstBoot = true;

RTC_DATA_ATTR uint32_t nextCommTime;
RTC_DATA_ATTR uint32_t nextSampleTime;
RTC_DATA_ATTR uint32_t commInterval;
RTC_DATA_ATTR uint32_t samplingInterval = DEFAULT_SAMPLING_INTERVAL;

RTC_DATA_ATTR MACAddress gatewayMAC = MACAddress::broadcast;

void SensorNode::commandPhase()
{
    Serial.println("COMMAND PHASE");
    size_t length;
    Serial.setTimeout(UART_PHASE_TIMEOUT * 1000);
    while (true)
    {
        char buffer[256];
        length = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        if (length >= 1 && buffer[length - 1] == '\r')
        {
            buffer[length - 1] = '\0';
        }
        else
        {
            buffer[length] = '\0';
        }
        Serial.println(buffer);
        if (strcmp(buffer, "") == 0 || strcmp(buffer, "exit") == 0 || strcmp(buffer, "close") == 0)
        {
            Serial.println("Exiting command phase...");
            return;
        }
        else if (strcmp(buffer, "discovery") == 0)
        {
            discoveryListen();
        }
        else if (strcmp(buffer, "ls") == 0 || strcmp(buffer, "list") == 0)
        {
            listFiles();
        }
        else if (strncmp(buffer, "print ", 6) == 0)
        {
            printFile(&buffer[6]);
        }
        else if (strcmp(buffer, "format") == 0)
        {
            Serial.println("Formatting flash memory (this can take some time)...");
            SPIFFS.format();
            Serial.println("Restarting ...");
            ESP.restart();
        }
        else if (strncmp(buffer, "echo ", 5) == 0)
        {
            Serial.println(&buffer[5]);
        }
        else
        {
            Serial.printf("Command '%s' not found or invalid argument(s) given.\n", buffer);
        }
    }
}
void SensorNode::listFiles()
{
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        Serial.println(file.path());
        file = root.openNextFile();
    }
    root.close();
    file.close();
}

void SensorNode::printFile(const char *filename)
{
    if (!SPIFFS.exists(filename))
    {
        Serial.printf("File '%s' does not exist.\n", filename);
        return;
    }
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file)
    {
        Serial.printf("Error while opening file '%s'\n", filename);
        return;
    }
    Serial.printf("%s with size %u bytes\n", filename, file.size());
    while (file.available())
    {
        Serial.write(file.read());
    }
    Serial.flush();
    file.close();
}

void SensorNode::deepSleep(float sleep_time)
{
    if (sleep_time <= 0)
        return;

    // For an unknown reason pin 15 was high by default, as pin 15 is connected to VPP with a 4.7k pull-up resistor it forced 3.3V on VPP when VPP was powered off.
    // Therefore we force pin 15 to a LOW state here.
    pinMode(15, OUTPUT);
    digitalWrite(15, LOW);

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep sleep
    if (sleep_time <= 30)
    {
        log->print(Logger::debug, "Using internal timer for deep sleep.");
        esp_sleep_enable_timer_wakeup((uint64_t)(sleep_time * 1000 * 1000));
    }
    else
    {
        log->print(Logger::debug, "Using RTC for deep sleep.");
        // We use the external RTC
        rtc->write_alarm_epoch(rtc->read_time_epoch() + (uint32_t)round(sleep_time));
        rtc->enable_alarm();

    }
    digitalWrite(16, LOW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc->getIntPin(), 0);
    esp_sleep_enable_ext1_wakeup((gpio_num_t)_BV(BOOT_PIN), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    lora.sleep();
    log->closeLogfile();
    SPIFFS.end();
    log->print(Logger::debug, "SPIFFS unmounted.");
    esp_deep_sleep_start();
}

void SensorNode::deepSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc->read_time_epoch();
    deepSleep((float)(time - ctime));
}

void SensorNode::lightSleep(float sleep_time)
{
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_time * 1000 * 1000);
    esp_light_sleep_start();
}

void SensorNode::lightSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc->read_time_epoch();
    lightSleep((float)(time - ctime));
}

void SensorNode::discoveryListen()
{
    log->print(Logger::info, "Awaiting discovery message...");
    Message hello = lora.receiveMessage(DISCOVERY_TIMEOUT, Message::HELLO);
    if (hello.isType(Message::ERROR))
    {
        log->print(Logger::error, "Error while awaiting discovery message from gateway. Aborting discovery listening.");
        return;
    }
    MACAddress gatewayMAC = hello.getSource();
    log->printf(Logger::info, "Gateway found at %s. Sending response message...", gatewayMAC.toString());
    lora.sendMessage(Message(Message::HELLO_REPLY, lora.getMACAddress(), gatewayMAC));
    log->print(Logger::debug, "Awaiting time config message...");
    Message timeConfigM = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::TIME_CONFIG, TIME_CONFIG_ATTEMPTS, gatewayMAC);
    if (timeConfigM.isType(Message::ERROR))
    {
        log->print(Logger::error, "Error while awaiting time config message from gateway. Aborting discovery listening.");
        return;
    }
    log->print(Logger::debug, "Time config message received. Sending TIME_ACK");
    TimeConfigMessage timeConfig = *static_cast<TimeConfigMessage *>(&timeConfigM);
    lora.sendMessage(Message(Message::ACK_TIME, lora.getMACAddress(), gatewayMAC));
    this->timeConfig(timeConfig);
    lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::REPEAT, 0, gatewayMAC);
}

void SensorNode::timeConfig(TimeConfigMessage& m)
{
    rtc->write_alarm_epoch(m.getCTime());
    nextCommTime = m.getCommTime();
    nextSampleTime = m.getSampleTime();
    samplingInterval = m.getSampleInterval();
    commInterval = m.getCommInterval();
    gatewayMAC = m.getSource();
}