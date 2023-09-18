#ifndef __SENSORNODE_H__
#define __SENSORNODE_H__

#include "Commands.h"
#include "MIRRAModule.h"
#include "config.h"
#include <vector>

class SensorNode : public MIRRAModule
{
public:
    SensorNode(const MIRRAPins& pins);
    void wake();

    struct Commands : CommonCommands
    {
        SensorNode* parent;
        Commands(SensorNode* parent) : parent{parent} {};

        CommandCode discovery();
        CommandCode sample();
        CommandCode printSample();
        CommandCode printSchedule();

        static constexpr auto getCommands()
        {
            return std::tuple_cat(CommonCommands::getCommands(),
                                  std::make_tuple(CommandAliasesPair(&Commands::discovery, "discovery"), CommandAliasesPair(&Commands::sample, "sample"),
                                                  CommandAliasesPair(&Commands::printSample, "printsample"),
                                                  CommandAliasesPair(&Commands::printSchedule, "printschedule")));
        }
    };

private:
    void discovery();
    void timeConfig(Message<TIME_CONFIG>& m);

    void addSensor(std::unique_ptr<Sensor>&& sensor);
    void initSensors();
    void clearSensors();

    Message<SENSOR_DATA> sampleAll();
    Message<SENSOR_DATA> sampleScheduled(uint32_t cTime);
    void updateSensorsSampleTimes(uint32_t cTime);
    void samplePeriod();

    void commPeriod();
    bool sendSensorMessage(Message<SENSOR_DATA>& message, const MACAddress& dest, bool& firstMessage);

    std::array<std::unique_ptr<Sensor>, MAX_SENSORS> sensors;
    size_t nSensors{0};
};

#endif
