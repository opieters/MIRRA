#include <MIRRAModule.h>

void MIRRAModule::prepare(const MIRRAPins& pins)
{
    Serial.begin(115200);
    Serial.println("Serial initialised.");
    gpio_hold_dis(static_cast<gpio_num_t>(pins.peripheralPowerPin));
    pinMode(pins.peripheralPowerPin, OUTPUT);
    digitalWrite(pins.peripheralPowerPin, HIGH);
    Wire.begin(pins.sdaPin, pins.sclPin); // i2c
    pinMode(pins.bootPin, INPUT);
    Serial.println("I2C wire initialised.");
    if (!LittleFS.begin())
    {
        Serial.println("Mounting LittleFS failed! Formatting and restarting ...");
        LittleFS.format();
        ESP.restart();
    }
    Serial.println("LittleFS initialsed.");
}

void MIRRAModule::end()
{
    Log::log.close();
    lora.sleep();
    LittleFS.end();
    Wire.end();
    digitalWrite(pins.peripheralPowerPin, LOW);
    gpio_hold_en(static_cast<gpio_num_t>(pins.peripheralPowerPin));
    Serial.flush();
    Serial.end();
}
MIRRAModule::MIRRAModule(const MIRRAPins& pins)
    : pins{pins}, rtc{pins.rtcIntPin, pins.rtcAddress}, lora{pins.csPin, pins.rstPin, pins.dio0Pin, pins.rxPin, pins.txPin}, commands{this, pins.bootPin, true}
{
    Log::log.setSerial(&Serial);
    Log::log.setLogfile(true);
    Log::log.setLogLevel(LOG_LEVEL);
    Serial.println("Logger initialised.");
    Log::info("Used ", LittleFS.usedBytes() / 1000, "KB of ", LittleFS.totalBytes() / 1000, "KB available on flash.");
}

void MIRRAModule::storeSensorData(const Message<SENSOR_DATA>& m, File& dataFile)
{
    dataFile.write(static_cast<uint8_t>(m.getLength()));
    dataFile.write(0); // mark not uploaded (yet)
    dataFile.write(&m.toData()[1], m.getLength() - 1);
}

void MIRRAModule::pruneSensorData(File&& dataFile, uint32_t maxSize)
{
    size_t fileSize = dataFile.size();
    if (fileSize <= maxSize)
        return;

    char fileName[strlen(dataFile.name()) + 2];
    snprintf(fileName, strlen(dataFile.name()) + 2, "/%s", dataFile.name());
    char tempFileName[strlen(fileName) - strlen(strrchr(fileName, '.')) + 4 + 1];
    strcpy(tempFileName, fileName);
    strcpy(strrchr(tempFileName, '.'), ".tmp");
    File dataFileTemp{LittleFS.open(tempFileName, "w", true)};

    while (dataFile.available())
    {
        uint8_t messageLength = sizeof(messageLength) + dataFile.peek();
        if (fileSize > maxSize)
        {
            fileSize -= messageLength;
            dataFile.seek(messageLength, SeekCur); // skip over the next message
        }
        else
        {
            uint8_t buffer[messageLength];
            dataFile.read(buffer, messageLength);
            dataFileTemp.write(buffer, messageLength);
        }
    }
    dataFile.close();
    LittleFS.remove(fileName);
    dataFileTemp.flush();
    Log::info("Sensor data pruned from ", fileSize / 1000, " KB to ", dataFileTemp.size() / 1000, " KB.");
    dataFileTemp.close();

    LittleFS.rename(tempFileName, fileName);
}

void MIRRAModule::deepSleep(uint32_t sleepTime)
{
    if (sleepTime <= 0)
    {
        Log::error("Sleep time was zero or negative! Sleeping one second to avert crisis.");
        return deepSleep(1);
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep
    // sleep
    if (sleepTime <= 30)
    {
        Log::debug("Using internal timer for deep sleep.");
        esp_sleep_enable_timer_wakeup((uint64_t)sleepTime * 1000 * 1000);
    }
    else
    {
        Log::debug("Using RTC for deep sleep.");
        rtc.writeAlarm(rtc.readTimeEpoch() + sleepTime);
        rtc.enableAlarm();
        esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc.getIntPin(), 0);
    }
    esp_sleep_enable_ext1_wakeup((gpio_num_t)_BV(this->pins.bootPin), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    Log::info("Good night.");
    this->end();
    esp_deep_sleep_start();
}

void MIRRAModule::deepSleepUntil(uint32_t untilTime)
{
    uint32_t cTime{rtc.getSysTime()};
    if (untilTime <= cTime)
    {
        deepSleep(0);
    }
    else
    {
        deepSleep(untilTime - cTime);
    }
}

void MIRRAModule::lightSleep(float sleepTime)
{
    if (sleepTime <= 0)
    {
        Log::error("Sleep time was zero or negative! Skipping to avert crisis.");
        return;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)sleepTime * 1000 * 1000);
    esp_light_sleep_start();
}

void MIRRAModule::lightSleepUntil(uint32_t untilTime)
{
    uint32_t cTime{rtc.getSysTime()};
    if (untilTime <= cTime)
    {
        return;
    }
    else
    {
        lightSleep(untilTime - cTime);
    }
}