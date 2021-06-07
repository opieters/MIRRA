## Setup

Upon boot the system will perform the following actions:

* Connect to WiFi
* (1st boot) get NTP time
* (1st boot) discovery mode
* (otherwise) readout mode

## Discovery mode

The following actions are performed:

* Send `hello` message
* Wait 10 seconds for resonse
* Repeat for 10 minutes
* Go to sleep mode

## Sleep mode

If enough time left before next readout, go into deep sleep. otherwise, go to light sleep or readout mode.

## Readout mode

* Contact each of the sensor nodes
* Wait for sensor data
* Acknowledge sensor data
* Send update on next communication interval
* (every hour) upload data to the server
* (otherwise) sleep

# Upload data to server

* connect to server over MQTT
* go to sleep mode

# UART readout state

# Error state