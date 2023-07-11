#ifndef __SENSORNODE_H__
#define __SENSORNODE_H__

#include "MIRRAModule.h"
#include "config.h"
#include <vector>

class SensorNode : public MIRRAModule
{
public:
    SensorNode(const MIRRAPins& pins);
    void wake();

    class Commands : public MIRRAModule::Commands<SensorNode>
    {
    private:
        void printSample();

    public:
        Commands(SensorNode* parent) : MIRRAModule::Commands<SensorNode>(parent){};
        CommandCode processCommands(char* command);
    };

private:
    void discovery();
    void timeConfig(Message<TIME_CONFIG>& m);

    void samplePeriod();

    void commPeriod();
    bool sendSensorMessage(Message<SENSOR_DATA>& message, const MACAddress& dest, bool& firstMessage);

    std::array<std::unique_ptr<Sensor>, MAX_SENSORS> sensors;
    size_t nSensors{0};
    void addSensor(std::unique_ptr<Sensor>&& sensor);
    void initSensors();
    void clearSensors();
    Message<SENSOR_DATA> sampleAll();
};

#endif
