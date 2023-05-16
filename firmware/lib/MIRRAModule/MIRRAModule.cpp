#include "MIRRAModule.h"

volatile bool commandPhaseFlag = false;

void IRAM_ATTR commandPhaseInterrupt()
{
    commandPhaseFlag = true;
}
MIRRAModule MIRRAModule::start(const MIRRAPins &pins)
{
    prepare(pins);
    return MIRRAModule(pins);
}

void MIRRAModule::prepare(const MIRRAPins &pins)
{
    Serial.begin(115200);
    Serial.println("Serial initialised.");
    Serial2.begin(9600);
    Wire.begin(pins.sda_pin, pins.scl_pin); // i2c
    Serial.println("I2C wire initialised.");
    if (!SPIFFS.begin(true))
    {
        Serial.println("Mounting SPIFFS failed! Restarting ...");
        ESP.restart();
    }
    Serial.println("SPIFFS initialsed.");
    pinMode(pins.boot_pin, INPUT);
    attachInterrupt(pins.boot_pin, commandPhaseInterrupt, FALLING);
}

MIRRAModule::MIRRAModule(const MIRRAPins &pins) : pins{pins}, rtc{pins.rtc_int_pin, pins.rtc_address}, log{LOG_LEVEL, LOG_FP, &rtc}, lora{&log, pins.cs_pin, pins.rst_pin, pins.dio0_pin, pins.rx_pin, pins.tx_pin} {}

void MIRRAModule::commandPhase()
{
    Serial.println("Press the BOOT pin to enter command phase ...");
    for (size_t i = 0; i < UART_PHASE_ENTRY_PERIOD * 10; i++)
    {
        if (commandPhaseFlag)
        {
            log.print(Logger::info, "Entering command phase...");
            break;
        }
        delay(100);
    }

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
        switch (processCommands(buffer))
        {
        case COMMAND_FOUND:
            break;
        case COMMAND_NOT_FOUND:
            Serial.printf("Command '%s' not found or invalid argument(s) given.\n", buffer);
            break;
        case COMMAND_EXIT:
            return;
        }
    }
}
MIRRAModule::CommandCode MIRRAModule::processCommands(char *command)
{
    if (strcmp(command, "") == 0 || strcmp(command, "exit") == 0 || strcmp(command, "close") == 0)
    {
        Serial.println("Exiting command phase...");
        return COMMAND_EXIT;
    }
    else if (strcmp(command, "ls") == 0 || strcmp(command, "list") == 0)
    {
        listFiles();
    }
    else if (strncmp(command, "print ", 6) == 0)
    {
        printFile(&command[6]);
    }
    else if (strncmp(command, "printhex ", 9) == 0)
    {
        printFile(&command[9], true);
    }
    else if (strcmp(command, "format") == 0)
    {
        Serial.println("Formatting flash memory (this can take some time)...");
        SPIFFS.format();
        Serial.println("Restarting ...");
        ESP.restart();
    }
    else if (strncmp(command, "echo ", 5) == 0)
    {
        Serial.println(&command[5]);
    }
    else
    {
        return COMMAND_NOT_FOUND;
    }
    return COMMAND_FOUND;
}
void MIRRAModule::listFiles()
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

void MIRRAModule::printFile(const char *filename, bool hex)
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
    if (hex)
    {
        while (file.available())
        {
            Serial.printf("%X", file.read());
        }
    }
    else
    {
        while (file.available())
        {
            Serial.write(file.read());
        }
    }

    Serial.flush();
    file.close();
}

void MIRRAModule::storeSensorData(SensorDataMessage &m, File &dataFile)
{
    uint8_t buffer[SensorDataMessage::max_length];
    m.toData(buffer);
    buffer[0] = 0; // mark not uploaded (yet)
    dataFile.write((uint8_t)m.getLength());
    dataFile.write(buffer, m.getLength());
}

void MIRRAModule::deepSleep(float sleep_time)
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
        log.print(Logger::debug, "Using internal timer for deep sleep.");
        esp_sleep_enable_timer_wakeup((uint64_t)(sleep_time * 1000 * 1000));
    }
    else
    {
        log.print(Logger::debug, "Using RTC for deep sleep.");
        // We use the external RTC
        rtc.write_alarm_epoch(rtc.read_time_epoch() + (uint32_t)round(sleep_time));
        rtc.enable_alarm();
    }
    digitalWrite(16, LOW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)rtc.getIntPin(), 0);
    esp_sleep_enable_ext1_wakeup((gpio_num_t)_BV(this->pins.boot_pin), ESP_EXT1_WAKEUP_ALL_LOW); // wake when BOOT button is pressed
    lora.sleep();
    log.closeLogfile();
    SPIFFS.end();
    log.print(Logger::debug, "SPIFFS unmounted.");
    esp_deep_sleep_start();
}

void MIRRAModule::deepSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc.read_time_epoch();
    deepSleep((float)(time - ctime));
}

void MIRRAModule::lightSleep(float sleep_time)
{
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_time * 1000 * 1000);
    esp_light_sleep_start();
}

void MIRRAModule::lightSleepUntil(uint32_t time)
{
    uint32_t ctime = rtc.read_time_epoch();
    lightSleep((float)(time - ctime));
}