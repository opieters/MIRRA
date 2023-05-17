#include "sensornode.h"

RTC_DATA_ATTR bool initialBoot = true;
extern volatile bool commandPhaseFlag;

RTC_DATA_ATTR uint32_t nextSampleTime;
RTC_DATA_ATTR uint32_t sampleInterval = DEFAULT_SAMPLING_INTERVAL;
RTC_DATA_ATTR uint32_t nextCommTime = -1;
RTC_DATA_ATTR uint32_t commInterval;
RTC_DATA_ATTR uint32_t commDuration;
RTC_DATA_ATTR MACAddress gatewayMAC;

const char sensorDataTempFN[] = "/data_temp.dat";

SensorNode::SensorNode(const MIRRAPins &pins) : MIRRAModule(MIRRAModule::start(pins))
{
    if (initialBoot)
    {
        if (SPIFFS.exists(DATA_FP))
            SPIFFS.remove(DATA_FP);
        File dataFile = SPIFFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();
        nextSampleTime = ((rtc.read_time_epoch() + sampleInterval) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
        discovery();
        initialBoot = false;
    }
}

void SensorNode::wake()
{
    log.print(Logger::debug, "Running wake()...");
    uint32_t ctime = rtc.read_time_epoch();
    if (ctime >= nextCommTime)
    {
        if (nextSampleTime >= nextCommTime && nextSampleTime <= (nextCommTime + (commDuration)))
        {
            commPeriod();
            samplePeriod();
        }
        else
        {
            commPeriod();
        }
    }
    ctime = rtc.read_time_epoch();
    if (ctime >= nextSampleTime)
    {
        samplePeriod();
    }
    log.printf(Logger::info, "Next sample in %us, next comm period in %us", nextSampleTime - ctime, nextCommTime - ctime);
    Serial.printf("Welcome! This is Sensor Node %s\n", lora.getMACAddress().toString());
    enterCommandPhase();
    ctime = rtc.read_time_epoch();
    if (ctime >= nextCommTime || ctime >= nextSampleTime)
        wake();
    log.print(Logger::debug, "Entering deep sleep...");
    deepSleepUntil((nextCommTime < nextSampleTime) ? nextCommTime : nextSampleTime);
}

MIRRAModule::CommandCode SensorNode::processCommands(char *command)
{
    CommandCode code = MIRRAModule::processCommands(command);
    if (code != CommandCode::COMMAND_NOT_FOUND)
        return code;
    if (strcmp(command, "discovery") == 0)
    {
        discovery();
        return CommandCode::COMMAND_FOUND;
    }
    return CommandCode::COMMAND_NOT_FOUND;
};

void SensorNode::discovery()
{
    log.print(Logger::info, "Awaiting discovery message...");
    Message hello = lora.receiveMessage<Message>(DISCOVERY_TIMEOUT, Message::HELLO);
    if (hello.isType(Message::ERROR))
    {
        log.print(Logger::error, "Error while awaiting discovery message from gateway. Aborting discovery listening.");
        return;
    }
    MACAddress gatewayMAC = hello.getSource();
    log.printf(Logger::info, "Gateway found at %s. Sending response message...", gatewayMAC.toString());
    lora.sendMessage(Message(Message::HELLO_REPLY, lora.getMACAddress(), gatewayMAC));
    log.print(Logger::debug, "Awaiting time config message...");
    TimeConfigMessage timeConfig = lora.receiveMessage<TimeConfigMessage>(TIME_CONFIG_TIMEOUT, Message::TIME_CONFIG, TIME_CONFIG_ATTEMPTS, gatewayMAC);
    if (timeConfig.isType(Message::ERROR))
    {
        log.print(Logger::error, "Error while awaiting time config message from gateway. Aborting discovery listening.");
        return;
    }
    this->timeConfig(timeConfig);
    log.print(Logger::debug, "Time config message received. Sending TIME_ACK");
    lora.sendMessage(Message(Message::ACK_TIME, lora.getMACAddress(), gatewayMAC));
    lora.receiveMessage<Message>(TIME_CONFIG_TIMEOUT, Message::REPEAT, 0, gatewayMAC);
}

void SensorNode::timeConfig(TimeConfigMessage &m)
{
    rtc.write_time_epoch(m.getCTime());
    nextSampleTime = (m.getSampleTime() == 0) ? nextSampleTime : m.getSampleTime();
    sampleInterval = (m.getSampleInterval() == 0) ? sampleInterval : m.getSampleInterval();
    nextCommTime = m.getCommTime();
    commInterval = m.getCommInterval();
    commDuration = m.getCommDuration();
    gatewayMAC = m.getSource();
    log.printf(Logger::info, "Sample interval: %u, Comm interval: %u, Comm duration: %u, Gateway MAC: %s", sampleInterval, commInterval, commDuration, gatewayMAC.toString());
}

void SensorNode::samplePeriod()
{
    log.print(Logger::info, "Sampling sensors...");
    uint32_t ctime = rtc.read_time_epoch();
    SensorValue values[SensorDataMessage::max_n_values];
    srand(ctime);
    for (SensorValue &value : values)
    {
        value = SensorValue(rand(), 0, rand());
    }
    SensorDataMessage message(lora.getMACAddress(), gatewayMAC, ctime, (rand() % SensorDataMessage::max_n_values) + 1, values);
    log.printf(Logger::debug, "Constructed Sensor Message with length %u", message.getLength());
    File data = SPIFFS.open(DATA_FP, FILE_APPEND);
    storeSensorData(message, data);
    data.close();
    data = SPIFFS.open(DATA_FP, FILE_READ);
    pruneSensorData(data);
    while (nextSampleTime <= ctime)
        nextSampleTime += sampleInterval;
}

void SensorNode::commPeriod()
{
    MACAddress destMAC = gatewayMAC; // avoid access to slow RTC memory
    log.printf(Logger::info, "Communicating with gateway %s ...", destMAC.toString());
    bool firstMessage = true;
    size_t messagesToSend = ((commDuration * 1000) - TIME_CONFIG_TIMEOUT) / SENSOR_DATA_TIMEOUT;
    log.printf(Logger::info, "Messages to send: %u", messagesToSend);
    File rdata = SPIFFS.open(DATA_FP, FILE_READ);
    File wdata = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    while (rdata.available())
    {
        uint8_t size = rdata.read();
        uint8_t buffer[size];
        rdata.read(buffer, size);
        if (buffer[0] == 0) // not yet uploaded
        {
            log.print(Logger::debug, "Reconstructing message from buffer...");
            buffer[0] = Message::Type::SENSOR_DATA << 1;
            SensorDataMessage message(buffer);
            message.setDest(destMAC);
            if ((messagesToSend == 1) || (!rdata.available())) // imperfect: assumes the last message in the data file is always one that has not been uploaded yet
            {
                log.print(Logger::debug, "Last sensor data message...");
                message.setLast();
            }
            buffer[0] = sendSensorMessage(message, destMAC, firstMessage);
            messagesToSend--;
        }
        wdata.write(size);
        wdata.write(buffer, size);
    }

    rdata.close();
    wdata.close();
    SPIFFS.remove(DATA_FP);
    SPIFFS.rename(sensorDataTempFN, DATA_FP);
    rdata = SPIFFS.open(DATA_FP, FILE_READ);
    pruneSensorData(rdata);
    rdata.close();
}

uint8_t SensorNode::sendSensorMessage(SensorDataMessage &message, MACAddress const &dest, bool &firstMessage)
{
    log.print(Logger::debug, "Sending data message...");
    if (firstMessage)
    {
        lora.sendMessage<SensorDataMessage>(message, 0); // gateway should already be listening for first message
        firstMessage = false;
    }
    else
    {
        lora.sendMessage<SensorDataMessage>(message);
    }
    log.print(Logger::debug, "Awaiting acknowledgement...");
    if (!message.isLast())
    {
        Message sensorAck = lora.receiveMessage<Message>(SENSOR_DATA_TIMEOUT, Message::Type::DATA_ACK, SENSOR_DATA_ATTEMPTS, dest);
        if (!sensorAck.isType(Message::Type::ERROR))
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
        TimeConfigMessage timeConfig = lora.receiveMessage<TimeConfigMessage>(TIME_CONFIG_TIMEOUT, Message::Type::TIME_CONFIG, TIME_CONFIG_ATTEMPTS, dest);
        if (!timeConfig.isType(Message::Type::ERROR))
        {
            this->timeConfig(timeConfig);
            lora.sendMessage<Message>(Message(Message::ACK_TIME, lora.getMACAddress(), dest));
            lora.receiveMessage<Message>(TIME_CONFIG_TIMEOUT, Message::REPEAT, 0, gatewayMAC);
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

void SensorNode::pruneSensorData(File &dataFile)
{
    size_t fileSize = dataFile.size();
    if (fileSize <= MAX_SENSORDATA_FILESIZE)
        return;
    File dataFileTemp = SPIFFS.open(sensorDataTempFN, FILE_WRITE, true);
    while (dataFile.available())
    {
        uint8_t message_length = sizeof(message_length) + dataFile.peek();
        if (fileSize > MAX_SENSORDATA_FILESIZE)
        {
            fileSize -= message_length;
            dataFile.seek(message_length, fs::SeekCur); // skip over the next message
        }
        else
        {
            uint8_t buffer[message_length];
            dataFile.read(buffer, message_length);
            dataFileTemp.write(buffer, message_length);
        }
    }
    dataFileTemp.flush();
    log.printf(Logger::info, "Sensor data pruned from %u bytes to %u bytes.", fileSize, dataFileTemp.size());
    dataFileTemp.close();
    SPIFFS.remove(DATA_FP);
    SPIFFS.rename(sensorDataTempFN, DATA_FP);
}