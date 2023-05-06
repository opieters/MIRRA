#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "LoRaModule.h"
#include "PCF2129_RTC.h"
#include <CommunicationCommon.h>
#include <FS.h>
#include "config.h"
#include <logging.h>
#include <vector>

class Node
{
private:
        MACAddress mac = MACAddress();
        uint32_t last_comm_time = 0;
        uint32_t next_comm_time = 0;
        uint32_t sampling_interval = 0;

public:
        Node(){};
        Node(MACAddress mac, uint32_t last_comm_time, uint32_t next_comm_time) : mac{mac}, last_comm_time{last_comm_time}, next_comm_time{next_comm_time} {}
        MACAddress getMACAddress() { return mac; }
        void updateLastCommTime(uint32_t ctime) { last_comm_time = ctime; };
        void updateNextCommTime(uint32_t time) { next_comm_time = time; };
        uint32_t getLastCommTime() { return last_comm_time; }
        uint32_t getNextCommTime() { return next_comm_time; }
};

class SensorNode
{
public:
        SensorNode(Logger *log, PCF2129_RTC *rtc);
        void sensorsInit()

        void wake();

        void commandPhase();
        void listFiles();
        void printFile(const char *filename);

        void deepSleep(float time);
        void deepSleepUntil(uint32_t time);
        void lightSleep(float time);
        void lightSleepUntil(uint32_t time);

        void discoveryListen();
        void timeConfig(TimeConfigMessage& m);

        void commPeriod();
        void storeSensorData(SensorDataMessage &m, File &dataFile);
        void pruneSensorData(File &dataFile);

        void printSensorData();

private:
        Logger *log;
        PCF2129_RTC *rtc;

        LoRaModule lora;

        std::vector<Sensor> sensors;
};

#endif