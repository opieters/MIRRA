#include "Sensor.h"
#include "Arduino.h"

uint32_t Sensor::adaptive_sample_interval_update(time_t current_sample_timepoint)
{
    Serial.println("General adaptive interval called.");
    // round to days
    time_t synchronised_sample_timepoint = (current_sample_timepoint / 60 / 60 / 24) * 60 * 60 * 24;
    time_t dtime = current_sample_timepoint - synchronised_sample_timepoint;
    synchronised_sample_timepoint = synchronised_sample_timepoint + (dtime / this->sample_interval) * this->sample_interval;

    if (synchronised_sample_timepoint != current_sample_timepoint)
    {
        return synchronised_sample_timepoint + sample_interval - current_sample_timepoint;
    }
    else
    {
        return this->sample_interval;
    }
}

SensorValue::SensorValue(uint8_t *data)
{
    this->tag = ((uint16_t *)data)[0];
    this->value = ((float *)data[sizeof(this->tag)])[0];
}

uint8_t *SensorValue::to_data(uint8_t *data)
{
    uint16_t *tag_p = (uint16_t *)data;
    tag_p[0] = this->tag;
    float *value_p = (float *)(&tag_p[1]);
    value_p[0] = this->value;
    return data;
}