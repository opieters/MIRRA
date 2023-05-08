#include "sensornode.h"

RTC_DATA_ATTR bool initialBoot = true;
extern volatile bool commandPhaseFlag;

RTC_DATA_ATTR uint32_t nextCommTime = -1;
RTC_DATA_ATTR uint32_t nextSampleTime;
RTC_DATA_ATTR uint32_t commInterval;
RTC_DATA_ATTR uint32_t samplingInterval = DEFAULT_SAMPLING_INTERVAL;

RTC_DATA_ATTR MACAddress gatewayMAC = MACAddress::broadcast;

SensorNode::SensorNode(const MIRRAPins &pins) : MIRRAModule(MIRRAModule::start(pins))
{
    if (initialBoot)
    {
        if (SPIFFS.exists(DATA_FP))
            SPIFFS.remove(DATA_FP);
        File dataFile = SPIFFS.open(DATA_FP, FILE_WRITE, true);
        dataFile.close();
        nextSampleTime = ((rtc.read_time_epoch() + DEFAULT_SAMPLING_INTERVAL) / (SAMPLING_ROUNDING)) * SAMPLING_ROUNDING;
        discovery();
        initialBoot = false;
    }
}

void SensorNode::wake()
{
    log.print(Logger::debug, "Running wake()...");
    commandPhase();
    log.print(Logger::debug, "Entering deep sleep...");
    deepSleepUntil((nextCommTime < nextSampleTime) ? nextCommTime : nextSampleTime);
}

MIRRAModule::CommandCode SensorNode::processCommands(char *command)
{
    CommandCode code = MIRRAModule::processCommands(command);
    if (code != CommandCode::COMMAND_NOT_FOUND)
        return code;
    if (strcmp(command, "discovery"))
    {
        discovery();
    }
};

void SensorNode::discovery()
{
    log.print(Logger::info, "Awaiting discovery message...");
    Message hello = lora.receiveMessage(DISCOVERY_TIMEOUT, Message::HELLO);
    if (hello.isType(Message::ERROR))
    {
        log.print(Logger::error, "Error while awaiting discovery message from gateway. Aborting discovery listening.");
        return;
    }
    MACAddress gatewayMAC = hello.getSource();
    log.printf(Logger::info, "Gateway found at %s. Sending response message...", gatewayMAC.toString());
    lora.sendMessage(Message(Message::HELLO_REPLY, lora.getMACAddress(), gatewayMAC));
    log.print(Logger::debug, "Awaiting time config message...");
    Message timeConfigM = lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::TIME_CONFIG, TIME_CONFIG_ATTEMPTS, gatewayMAC);
    if (timeConfigM.isType(Message::ERROR))
    {
        log.print(Logger::error, "Error while awaiting time config message from gateway. Aborting discovery listening.");
        return;
    }
    log.print(Logger::debug, "Time config message received. Sending TIME_ACK");
    TimeConfigMessage timeConfig = *static_cast<TimeConfigMessage *>(&timeConfigM);
    lora.sendMessage(Message(Message::ACK_TIME, lora.getMACAddress(), gatewayMAC));
    this->timeConfig(timeConfig);
    lora.receiveMessage(TIME_CONFIG_TIMEOUT, Message::REPEAT, 0, gatewayMAC);
}

void SensorNode::timeConfig(TimeConfigMessage &m)
{
    rtc.write_alarm_epoch(m.getCTime());
    nextCommTime = m.getCommTime();
    nextSampleTime = (m.getSampleTime() == 0) ? nextSampleTime : m.getSampleTime();
    samplingInterval = (m.getSampleInterval() == 0) ? samplingInterval : m.getSampleInterval();
    commInterval = m.getCommInterval();
    gatewayMAC = m.getSource();
}