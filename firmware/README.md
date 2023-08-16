# MIRRA Firmware

This folder contains all the firmwares necessary to set up a MIRRA installation. It is set up as a single PlatformIO project where each different firmware is configured as a seperate PlatformIO 'environment'. 
To initiate a firmware upload, hold the boot button and press the reset button (multiple tries may be necessary), then initiate the upload from the PlatformIO console.

## On RESET:
Local filesystem data stored in the module will be cleared, but logs will remain intact. 

Nodes will listen for any gateway discovery broadcasts for 5 minutes. If no gateway is found in this time, the node will default to a set sampling interval, but will never communicate. Manual `discovery` can always be initialised from the command line interface.

Gateways will attempt to connect to WiFi and configure their RTC on initial startup. If the preprogrammed credentials for WiFi are incorrect, this will inevitably result in failure and manual `wifi` and `rtc` will have to be initialised from the command line interface.

## Log Level

The gateway and the sensor nodes publish log messages to both the serial monitor (if one is connected) and the local filesystem. These messages can be filtered according to log level.
It is recommended to set the log level to either **INFO** or **ERROR**, as **DEBUG** tends to fill up the filesystem very quickly, rendering the system inoperable. Alternatively, logging to file can be disabled.

## Command Line Interface

Both the gateway and the sensor nodes can be interacted with via a serial monitor using a command line interface, either using PlatformIO's built in monitor command or a terminal emulator with similar functionality like PuTTY.

To enter a given module's command mode, hold the BOOT button. This will wake up the module and, if held properly, make it enter the command phase. On success, the module should print a message signifying that command phase has been entered.

Note: While in command line mode, the module cannot execute any other function, meaning it can miss sampling or communication periods. While the command phase should automatically timeout after one minute, it is still recommended to `exit` command mode and let the module return to sleep as soon as possible.

### Common Commands

The following commands are available to both gateway and nodes:

- `exit` or `close`: Exits command phase. The module will most likely enter sleep right after this.

- `ls` or `list`: Lists all the files currently housed in the filesystem.

- `print ARG`: Prints a file with name `ARG` to the serial output.

- `printhex ARG`: Prints a file with name `ARG` to the serial output, interpreted in hexadecimal notation. Useful for pure binary files (ending in .dat).

- `rm ARG`: Removes the file `ARG` from the filesystem if it exists and if it is not currently in use.

- `touch ARG`: Creates an empty file with the name `ARG` on the filesystem.

- `format`: Formats the filesystem. This effectively removes all the data stored in flash, and subsequently resets the module.

-  `echo ARG`: echoes `ARG` to the serial output.

### Gateway Commands

The following commands are exclusive to the gateway:

- `discovery`: Broadcasts initial discovery message (make sure RTC and WiFi are properly configured at this point!).

- `discoveryloop ARG`: Sends a discovery message every 2.5 minutes, `ARG` times.

- `rtc`: Attempts to configure the RTC time using WiFi.

- `wifi`: Enters wifi configuration mode, in which the wifi SSID and password can be entered and checked. If the new credentials are correct and connection is sucessful, the gateway will remember and henceforth use the given credentials whenever connecting to WiFi.

- `printschedule` : Prints scheduling information about the connected nodes, including MAC address, next comm time, sample interval and max number of messages per comm period.

### Sensor Node Commands

The following commands are exclusive to the sensor nodes:

- `discovery`: Enter listening mode for a discovery message from a gateway for 5 minutes. It is highly advised to only have one sensor node in listening mode at a time, as multiple nodes attempting to answer the broadcast will likely fail.

- `sample` : Forcefully initiates a sample period. This samples the sensors and stores the resulting data in the local data file, which will be sent to the gateway. This may impact sensor scheduling.

- `printsample` : Samples the sensors and prints each sample. Unlike `sample`, this does not forward any data to the local data file and will not impact sensor scheduling or communications.

- `printschedule` : Prints scheduling information about the sensors, including tag identifier and next sample time.

