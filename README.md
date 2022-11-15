# MIRRA 

Microclimate Instrument for Real-time Remote Applications

⚠️ This is a research project. Limited testing has been performed and reliability of data storage/sensor readout is not guaranteed. Use with caution. ⚠️

## 3D Designs

The radiation is 3D printed and designed using OpenSCAD. All design files can eb found in `3d_printing`. They can be printed using an extrusion printer, with minimal post-processign (no supports).

## PCB designs

All PCBs are designed with KiCAD and located in the `pcb_boards` directory. Components also contain ordering codes for Mouser and/or Farnell, such that the design is completely rebuilable.

## Embedded Code

The software for the sensor node and gateway are located in the `firmware` folder. The easiest way to build and upload the code is using PlaformIO. 

## Server / web-interface

Use docker-compose to start the webserver and related components (such as MQTT server, graphena...) from the `webserver` folder. Some services are password protected, the passwords are (for now) also included in the docker-compose file.

## Licence

Apache-2.0 License, see LICENSE file.
