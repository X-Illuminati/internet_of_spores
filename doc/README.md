# Documentation

## Table of Contents
* [System Architecture](system_architecture.md)
* Sensor Design
  * [Sensor Hardware](sensor_hardware.md)
  * [Sensor Software Architecture](software_architecture.md)
  * [Sensor Software Design](software_design_detail.md)
* Server Design
  * [Server Software Architecture](server_architecture.md)
  * [Server Software Design](server_design_detail.md)
* [User Guide](user_guide.md)

## Folder Structure
* [Top Level](../)
  * [README](../README.md)
  * [LICENSE](../LICENSE)
  * [internet_of_spores.ino](../internet_of_spores.ino) - Arduino Project File
  * C++ Modules for the Sensor Software
  * [flash.sh](../flash.sh) - Script to help program the ESP8266 over a serial port
  * [monitor.sh](../monitor.sh) - Script to monitor the ESP8266 serial port for debugging
  * [doc/](../doc/) - Documentation for the project
    * [README.md](README.md) - Documentation overview
    * [user_guide.md](user_guide.md) - Usage instructions
    * [drawio/](drawio/) - Draw.io Diagram Files
    * [photos/](photos/)
    * [renders/](renders/)
    * [screenshots/](screenshots/)
  * [firmware/](../firmware/) - Firmware images for the sensor nodes
    * ...-iotsp-tethered.bin - Firmware image for USB powered sensor nodes
    * ...-iotsp-battery.bin - Firmware image for battery-powered sensor nodes
    * ...-vcc-calibration.bin - Firmware image that rapidly reports battery level for calibration purposes
  * [kicad/](../kicad/) - KiCad projects
    * [EPD_1in9/](../kicad/EPD_1in9/) - KiCad project for the E-Paper Display Board
      * [EPD_1in9_schematic.pdf](../kicad/EPD_1in9/EPD_1in9_schematic.pdf) - PDF version of the schematic
  * [node-red/](../node-red/) - Node-RED flows and helper scripts
  * [openscad/](../openscad/) - OpenSCAD projects
    * [EPD_1in9.scad](../openscad/EPD_1in9.scad) - 3D Model for the Waveshare 1.9" E-Paper Display

