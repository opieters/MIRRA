#ifndef __SENSORNODE_H__
#define __SENSORNODE_H__

#include "MIRRAModule.h"
#include "config.h"
#include <vector>

class SensorNode : public MIRRAModule
{
public:
        SensorNode(const MIRRAPins &pins);

        void wake();

        CommandCode processCommands(char *command);

        void discovery();
        void timeConfig(TimeConfigMessage &m);

        void samplePeriod();

        void commPeriod();
        uint8_t sendSensorMessage(SensorDataMessage &message, MACAddress const &dest, bool &firstMessage);
        void pruneSensorData(File &dataFile);

        void printSensorData();

private:
        std::vector<Sensor> sensors;
};

#endif
