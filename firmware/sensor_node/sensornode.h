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

        /// @brief Forces a discovery period.
        CommandCode discovery();
        /// @brief Forcefully initiates a sample period. This samples the sensors and stores the resulting data in the local data file, which will be sent to
        /// the gateway. This may impact sensor scheduling.
        CommandCode sample();
        /// @brief Samples the sensors and prints each sample. Unlike sample, this does not forward any data to the local data file and will not impact sensor
        /// scheduling or communications.
        CommandCode printSample();
        /// @brief Prints scheduling information about the sensors, including tag identifier and next sample time.
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
    /// @brief Enter listening mode for a discovery message from a gateway for 5 minutes.
    void discovery();
    /// @brief Configures this node with a time config message.
    /// @param m Time Config message used to saturate the communication attributes.
    void timeConfig(Message<TIME_CONFIG>& m);

    /// @brief Loads a sensor and its associated scheduled sampling time.
    /// @param sensor Sensor to load.
    void addSensor(std::unique_ptr<Sensor>&& sensor);
    /// @brief Grouping method used to load all sensors. Alter sensor configurations here.
    void initSensors();
    /// @brief Clears all sensors and saves their associated scheduled sampling times.
    void clearSensors();

    /// @brief Samples all sensors irregardless of scheduling.
    /// @return The sensor data message constructed from the sampled sensors.
    Message<SENSOR_DATA> sampleAll();
    /// @brief Samples all sensors scheduled at the given time.
    /// @param cTime Time for which all sampled sensors are scheduled.
    /// @return The sensor data message constructed from the sampled sensors.
    Message<SENSOR_DATA> sampleScheduled(uint32_t cTime);
    /// @brief Updates each sensors' scheduled sampling time if it has expired, given the current time.
    /// @param cTime The current time.
    void updateSensorsSampleTimes(uint32_t cTime);
    /// @brief Initiates a sampling period.
    void samplePeriod();

    /// @brief Uploads sensor data messages to the gateway, and marks them as uploaded if successful.
    void commPeriod();
    /// @brief Sends a single sensor message to the gateway, handling both acknowledgement and, if it is the last message, time configuration.
    /// @param message The message to send.
    /// @param dest The gateway MAC address.
    /// @param firstMessage Whether the message is the first in the 'conversation' or not.
    /// @return Whether the sent message was successfully acknowledged or not.
    bool sendSensorMessage(Message<SENSOR_DATA>& message, const MACAddress& dest, bool& firstMessage);

    std::array<std::unique_ptr<Sensor>, MAX_SENSORS> sensors;
    size_t nSensors{0};
};

#endif
