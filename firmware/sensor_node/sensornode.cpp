#include "sensornode.h"

#include <BatterySensor.h>
#include <RandomSensor.h>

RTC_DATA_ATTR bool initialBoot = true;

RTC_DATA_ATTR uint32_t nextSampleTime;
RTC_DATA_ATTR uint32_t sampleInterval = DEFAULT_SAMPLING_INTERVAL;
RTC_DATA_ATTR uint32_t nextCommTime = -1;
RTC_DATA_ATTR uint32_t commInterval;
RTC_DATA_ATTR uint32_t commDuration;
RTC_DATA_ATTR MACAddress gatewayMAC;

SensorNode::SensorNode(const MIRRAPins& pins) : MIRRAModule(MIRRAModule::start(pins))
{
    if (initialBoot)
    {
        if (LittleFS.exists(DATA_FP))
            LittleFS.remove(DATA_FP);
        File dataFile = LittleFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();
        nextSampleTime = ((rtc.read_time_epoch() + sampleInterval) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
        // discovery();
        initialBoot = false;
    }
}

void SensorNode::wake()
{
    log.print(Logger::debug, "Running wake()...");
    uint32_t ctime{rtc.read_time_epoch()};
    if (ctime >= WAKE_COMM_PERIOD(nextCommTime))
    {
        commPeriod();
    }
    ctime = rtc.read_time_epoch();
    if (ctime >= nextSampleTime)
    {
        samplePeriod();
    }
    log.printf(Logger::info, "Next sample in %us, next comm period in %us", nextSampleTime - ctime, nextCommTime - ctime);
    Serial.printf("Welcome! This is Sensor Node %s\n", lora.getMACAddress().toString());
    Commands(this).prompt();
    ctime = rtc.read_time_epoch();
    if (ctime >= nextCommTime || ctime >= nextSampleTime)
        wake();
    log.print(Logger::debug, "Entering deep sleep...");
    deepSleepUntil((nextCommTime < nextSampleTime) ? WAKE_COMM_PERIOD(nextCommTime) : nextSampleTime);
}

void SensorNode::discovery()
{
    log.print(Logger::info, "Awaiting discovery message...");
    auto hello = lora.receiveMessage<HELLO>(DISCOVERY_TIMEOUT);
    if (!hello)
    {
        log.print(Logger::error, "Error while awaiting discovery message from gateway. Aborting discovery listening.");
        return;
    }
    const MACAddress& gatewayMAC = hello->getSource();
    log.printf(Logger::info, "Gateway found at %s. Sending response message...", gatewayMAC.toString());
    lora.sendMessage(Message<HELLO_REPLY>(lora.getMACAddress(), gatewayMAC));
    log.print(Logger::debug, "Awaiting time config message...");
    auto timeConfig = lora.receiveMessage<TIME_CONFIG>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, gatewayMAC);
    if (!timeConfig)
    {
        log.print(Logger::error, "Error while awaiting time config message from gateway. Aborting discovery listening.");
        return;
    }
    this->timeConfig(*timeConfig);
    log.print(Logger::debug, "Time config message received. Sending TIME_ACK");
    lora.sendMessage(Message<ACK_TIME>(lora.getMACAddress(), gatewayMAC));
    lora.receiveMessage<REPEAT>(TIME_CONFIG_TIMEOUT, 0, gatewayMAC);
}

void SensorNode::timeConfig(Message<TIME_CONFIG>& m)
{
    rtc.write_time_epoch(m.getCTime());
    nextSampleTime = (m.getSampleTime() == 0) ? nextSampleTime : m.getSampleTime();
    sampleInterval = (m.getSampleInterval() == 0) ? sampleInterval : m.getSampleInterval();
    nextCommTime = m.getCommTime();
    commInterval = m.getCommInterval();
    commDuration = m.getCommDuration();
    gatewayMAC = m.getSource();
    log.printf(Logger::info, "Sample interval: %u, Comm interval: %u, Comm duration: %u, Gateway MAC: %s", sampleInterval, commInterval, commDuration,
               gatewayMAC.toString());
}

void SensorNode::addSensor(std::unique_ptr<Sensor>&& sensor)
{
    if (nSensors > MAX_SENSORS)
        return;
    sensor->setup();
    sensors[nSensors] = std::move(sensor);
    nSensors++;
}

void SensorNode::initSensors()
{
    addSensor(std::make_unique<RandomSensor>(rtc.read_time_epoch()));
    addSensor(std::make_unique<BatterySensor>(35, 33));
}

void SensorNode::clearSensors()
{
    for (size_t i{0}; i < nSensors; i++)
    {
        sensors[nSensors].reset();
    }
    nSensors = 0;
}

Message<SENSOR_DATA> SensorNode::sampleAll()
{
    initSensors();
    log.print(Logger::info, "Sampling sensors...");
    for (size_t i = 0; i < nSensors; i++)
    {
        sensors[i]->startMeasurement();
    }
    uint32_t ctime = rtc.read_time_epoch();
    std::array<SensorValue, Message<SENSOR_DATA>::maxNValues> values;
    for (size_t i = 0; i < nSensors; i++)
    {
        values[i] = sensors[i]->getMeasurement();
    }
    return Message<SENSOR_DATA>(lora.getMACAddress(), gatewayMAC, ctime, static_cast<uint8_t>(nSensors), values);
}
void SensorNode::samplePeriod()
{
    Message<SENSOR_DATA> message{sampleAll()};
    log.printf(Logger::debug, "Constructed Sensor Message with length %u", message.getLength());
    File data = LittleFS.open(DATA_FP, FILE_APPEND);
    storeSensorData(message, data);
    data.close();
    uint32_t ctime{message.getCTime()};
    while (nextSampleTime <= ctime)
        nextSampleTime += sampleInterval;
}

void SensorNode::commPeriod()
{
    MACAddress destMAC{gatewayMAC}; // avoid access to slow RTC memory
    log.printf(Logger::info, "Communicating with gateway %s ...", destMAC.toString());
    size_t messagesToSend{((commDuration * 1000) - TIME_CONFIG_TIMEOUT) / SENSOR_DATA_TIMEOUT};
    log.printf(Logger::info, "Max messages to send: %u", messagesToSend);
    size_t firstNonUploaded{0};
    std::vector<Message<SENSOR_DATA>> data;
    data.reserve(messagesToSend);
    bool uploadSuccess[messagesToSend];
    File dataFile = LittleFS.open(DATA_FP, "r+");
    while (dataFile.available())
    {
        uint8_t size{dataFile.read()};
        uint8_t buffer[size];
        dataFile.read(buffer, size);
        if (buffer[0] == 0 && messagesToSend > 0) // not yet uploaded
        {
            if (firstNonUploaded == 0)
                firstNonUploaded = dataFile.position();
            log.print(Logger::debug, "Reconstructing message from buffer...");
            Message<SENSOR_DATA>& message{Message<SENSOR_DATA>::fromData(buffer)};
            message.setType(SENSOR_DATA);
            if ((messagesToSend == 1) ||
                (!dataFile.available())) // imperfect: assumes the last message in the data file is always one that has not been uploaded yet
            {
                log.print(Logger::debug, "Last sensor data message...");
                message.setLast();
            }
            data.push_back(message);
            messagesToSend--;
        }
    }
    bool firstMessage{true};
    for (size_t i{0}; i < data.size(); i++)
    {
        uploadSuccess[i] = sendSensorMessage(data[i], destMAC, firstMessage);
    }
    dataFile.seek(firstNonUploaded);
    for (size_t i{0}; i < data.size();)
    {
        uint8_t size{dataFile.read()};
        if (dataFile.peek() == 0)
        {
            dataFile.write(static_cast<uint8_t>(uploadSuccess[i]));
            i++;
        }
        dataFile.seek(size, SeekCur);
    }
    dataFile.seek(0);
    pruneSensorData(std::move(dataFile), MAX_SENSORDATA_FILESIZE);
}

bool SensorNode::sendSensorMessage(Message<SENSOR_DATA>& message, MACAddress const& dest, bool& firstMessage)
{
    log.print(Logger::debug, "Sending data message...");
    if (firstMessage)
    {
        lightSleepUntil(nextCommTime);
        lora.sendMessage(message, 0); // gateway should already be listening for first message
        firstMessage = false;
    }
    else
    {
        lora.sendMessage(message);
    }
    log.print(Logger::debug, "Awaiting acknowledgement...");
    if (!message.isLast())
    {
        auto dataAck = lora.receiveMessage<ACK_DATA>(SENSOR_DATA_TIMEOUT, SENSOR_DATA_ATTEMPTS, dest);
        if (dataAck)
        {
            return 1;
        }
        else
        {
            log.print(Logger::error, "Error while uploading to gateway.");
            return 0;
        }
    }
    else
    {
        auto timeConfig = lora.receiveMessage<TIME_CONFIG>(TIME_CONFIG_TIMEOUT, TIME_CONFIG_ATTEMPTS, dest);
        if (timeConfig)
        {
            this->timeConfig(*timeConfig);
            lora.sendMessage(Message<ACK_TIME>(lora.getMACAddress(), dest));
            lora.receiveMessage<REPEAT>(TIME_CONFIG_TIMEOUT, 0, gatewayMAC);
            return 1;
        }
        else
        {
            log.print(Logger::error, "Error while receiving new time config from gateway. Guessing next comm and sample time based on past interaction...");
            nextCommTime += commInterval;
            return 0;
        }
    }
}

MIRRAModule::Commands<SensorNode>::CommandCode SensorNode::Commands::processCommands(char* command)
{
    CommandCode code = MIRRAModule::Commands<SensorNode>::processCommands(command);
    if (code != CommandCode::COMMAND_NOT_FOUND)
        return code;
    if (strcmp(command, "discovery") == 0)
    {
        parent->discovery();
    }
    else if (strcmp(command, "sample") == 0)
    {
        parent->samplePeriod();
    }
    else if (strcmp(command, "printsample") == 0)
    {
        printSample();
    }
    else
    {
        return CommandCode::COMMAND_NOT_FOUND;
    }
    return CommandCode::COMMAND_FOUND;
};

void SensorNode::Commands::printSample()
{
    Message<SENSOR_DATA> message{parent->sampleAll()};
    auto& values = message.getValues();
    Serial.println("TAG\tVALUE");
    for (size_t i{0}, nValues{message.getNValues()}; i < nValues; i++)
    {
        Serial.printf("%u\t%f\n", values[i].tag, values[i].value);
    }
    parent->clearSensors();
}