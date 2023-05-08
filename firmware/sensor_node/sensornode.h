#ifndef __SENSORNODE_H__
#define __SENSORNODE_H__

#include "MIRRAModule.h"
#include "config.h"
#include <vector>

class SensorNode : public MIRRAModule
{
public:
        SensorNode(const MIRRAPins &pins);
        void sensorsInit();

        void wake();

        CommandCode processCommands(char *command);

        void discovery();
        void timeConfig(TimeConfigMessage &m);

        void commPeriod();
        void storeSensorData(SensorDataMessage &m, File &dataFile);
        void pruneSensorData(File &dataFile);

        void printSensorData();

private:
        std::vector<Sensor> sensors;
};

#endif
