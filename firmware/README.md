# MIRRA Firmware

This folder contains all the firmwares necessary to set up a MIRRA installation. It is set up as a single PlatformIO project where each different firmware is configured as a seperate PlatformIO 'environment'. 
To initiate a firmware upload, hold the boot button and press the reset button (multiple tries may be necessary), then initiate the upload from the PlatformIO console.

## On RESET:
Local filesystem data stored in the module will be cleared, but logs will remain intact. 

Nodes will listen for any gateway discovery broadcasts for 5 minutes. If no gateway is found in this time, the node will default to a set sampling interval, but will never communicate. Manual `discovery` can always be initialised from the command line interface.

Gateways will attempt to connect to WiFi and configure their RTC. If the preprogrammed credentials for WiFi are incorrect, this will inevitably result in failure and manual `wifi` and `rtc` will have to be initialised from the command line interface.

## Log Level

The gateway and the sensor nodes publish log messages to both the serial monitor (if one is connected) and the local filesystem. These messages can be filtered according to log level.
It is recommended to set the log level to either **INFO** or **ERROR**, as **DEBUG** tends to fill up the filesystem very quickly, rendering the system inoperable. Alternatively, logging to file can be disabled.

## Command Line Interface

Both the gateway and the sensor nodes can be interacted with via a serial monitor using a command line interface, either using PlatformIO's built in monitor command or a terminal emulator with similar functionality like PuTTY.

To enter a given module's command mode, press the BOOT button twice (first to wakeup the module, second to actually enter command mode). The module should print a message signifying that command mode has been entered.

The input given to the module is currently not echoed on a per-character basis, meaning that any input the module receives will only be printed upon pressing enter.

The following commands are available to both gateway and nodes:

- `'ENTER'` or `exit` or `close`: Exits command phase. The module will most likely enter sleep right after this.

- `ls` or `list`: Lists all the files currently housed in the filesystem.

- `print ARG`: Prints a file with name `ARG` to the serial output.

- `printhex ARG`: Prints a file with name `ARG` to the serial output, interpreted as hexadecimal notation. Useful for pure binary files (ending in .dat).

- `format`: Formats the filesystem. This effectively removes all the data stored in flash, and subsequently resets the module.

-  `echo ARG`: echoes `ARG` to the serial output.

- `discovery`: In case of a sensor node: enter listening mode for discovery message for 5 minutes. In case of gateway: broadcast discovery message (make sure RTC and WiFi are properly configured at this point!) . It is highly advised to only have one sensor node in listening mode per gateway broadcast, as multiple nodes attempting to answer the broadcast will likely fail.

The following commands are exclusive to the gateway:
- `rtc`: Attempts to configure the RTC time using WiFi.

- `wifi`: Enters wifi configuration mode, in which the wifi SSID and password can be entered and checked. If the new credentials are correct and connection is sucessful, the gateway will remember and henceforth use the given credentials whenever connecting to WiFi.



Note: While in command line mode, the module cannot execute any other function, meaning it can miss sampling or communication periods. When no longer in use, it is thus recommended to exit command mode and let the module return to sleep as soon as possible.
