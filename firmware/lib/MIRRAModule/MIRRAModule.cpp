#include <MIRRAModule.h>

volatile bool commandPhaseFlag{false};

void IRAM_ATTR commandPhaseInterrupt() { commandPhaseFlag = true; }

void MIRRAModule::prepare(const MIRRAPins& pins)
{
    Serial.begin(115200);
    Serial.println("Serial initialised.");
    gpio_hold_dis(static_cast<gpio_num_t>(pins.per_power_pin));
    pinMode(pins.per_power_pin, OUTPUT);
    digitalWrite(pins.per_power_pin, HIGH);
    Wire.begin(pins.sda_pin, pins.scl_pin); // i2c
    Serial.println("I2C wire initialised.");
    if (!LittleFS.begin())
    {
        Serial.println("Mounting LittleFS failed! Formatting and restarting ...");
        LittleFS.format();
        ESP.restart();
    }
    Serial.println("LittleFS initialsed.");
    pinMode(pins.boot_pin, INPUT);
    attachInterrupt(pins.boot_pin, commandPhaseInterrupt, FALLING);
}

void MIRRAModule::end()
{
    Log::log.close();
    lora.sleep();
    LittleFS.end();
    Wire.end();
    digitalWrite(pins.per_power_pin, LOW);
    gpio_hold_en(static_cast<gpio_num_t>(pins.per_power_pin));
    Serial.end();
}
MIRRAModule::MIRRAModule(const MIRRAPins& pins)
    : pins{pins}, rtc{pins.rtc_int_pin, pins.rtc_address}, lora{pins.cs_pin, pins.rst_pin, pins.dio0_pin, pins.rx_pin, pins.tx_pin}
{
    Log::log.setSerial(&Serial);
    Log::log.setLogfile(true);
    Log::log.setLogLevel(LOG_LEVEL);
    Serial.println("Logger initialised.");
}

void MIRRAModule::storeSensorData(Message<SENSOR_DATA>& m, File& dataFile)
{
    dataFile.write(static_cast<uint8_t>(m.getLength()));
    dataFile.write(0); // mark not uploaded (yet)
    dataFile.write(&m.toData()[1], m.getLength() - 1);
}

void MIRRAModule::pruneSensorData(File&& dataFile, uint32_t maxSize)
{
    char fileName[strlen(dataFile.name()) + 2];
    snprintf(fileName, strlen(dataFile.name()) + 2, "/%s", dataFile.name());
    char tempFileName[strlen(fileName) - strlen(strrchr(fileName, '.')) + 4 + 1];
    strcpy(tempFileName, fileName);
    strcpy(strrchr(tempFileName, '.'), ".tmp");

    size_t fileSize = dataFile.size();
    if (fileSize <= maxSize)
        return;
    File dataFileTemp{LittleFS.open(tempFileName, "w", true)};
    while (dataFile.available())
    {
        uint8_t message_length = sizeof(message_length) + dataFile.peek();
        if (fileSize > maxSize)
        {
            fileSize -= message_length;
            dataFile.seek(message_length, SeekCur); // skip over the next message
        }
        else
        {
            uint8_t buffer[message_length];
            dataFile.read(buffer, message_length);
            dataFileTemp.write(buffer, message_length);
        }
    }
    LittleFS.remove(fileName);
    dataFile.close();
    dataFileTemp.flush();
    Log::info("Sensor data pruned from ", fileSize / 1000, " KB to ", dataFileTemp.size() / 1000, " KB.");
    dataFileTemp.close();

    LittleFS.rename(tempFileName, fileName);
}

void MIRRAModule::deepSleep(uint32_t sleep_time)
{
    if (sleep_time <= 0)
    {
        Log::error("Sleep time was zero or negative! Sleeping one second to avert crisis.");
        return deepSleep(1);
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // The external RTC only has a alarm resolution of 1s, to be more accurate for times lower than 10s the internal oscillator will be used to wake from deep
    // sleep
    if (sleep_time <= 30)
    {
        Log::debug("Using internal timer for deep sleep.");
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_time * 1000 * 1000);
    }
    else
    {
        Log::debug("Using RTC for deep sleep.");
        rtc.write_alarm_epoch(rtc.read_time_epoch() + sleep_time);
        rtc.enable_alarm();
        esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc.getIntPin(), 0);
    }
    esp_sleep_enable_ext1_wakeup((gpio_num_t)_BV(this->pins.boot_pin), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    Log::info("Good night.");
    esp_deep_sleep_start();
}

void MIRRAModule::deepSleepUntil(uint32_t untilTime)
{
    uint32_t ctime{time(nullptr)};
    if (untilTime <= ctime)
    {
        deepSleep(0);
    }
    else
    {
        deepSleep(untilTime - ctime);
    }
}

void MIRRAModule::lightSleep(float sleep_time)
{
    if (sleep_time <= 0)
    {
        Log::error("Sleep time was zero or negative! Skipping to avert crisis.");
        return;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_time * 1000 * 1000);
    esp_light_sleep_start();
}

void MIRRAModule::lightSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc.read_time_epoch();
    if (time <= ctime)
    {
        return;
    }
    else
    {
        lightSleep(time - ctime);
    }
}