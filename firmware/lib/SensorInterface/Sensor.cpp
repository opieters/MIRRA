#include "Sensor.h"
#include "Arduino.h"

uint32_t Sensor::adaptive_sample_interval_update(uint32_t current_sample_timepoint)
{
    Serial.println("General adaptive interval called.");
    // round to days
    time_t synchronised_sample_timepoint = (current_sample_timepoint / 60 / 60 / 24) * 60 * 60 * 24;
    time_t dtime = current_sample_timepoint - synchronised_sample_timepoint;
    synchronised_sample_timepoint = synchronised_sample_timepoint + (dtime / this->sample_interval) * this->sample_interval;
    return synchronised_sample_timepoint + sample_interval - current_sample_timepoint;
}