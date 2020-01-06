# MStar-WLAN

This project includes both hardware design and software for interfacing to a [Morningstar](https://www.morningstarcorp.com/) solar controller via wireless LAN (WiFi). It's 99% working, but still progressing. The core is an ESP8266 based WEMOS D1 Mini Pro board (about $4 via AliExpress and wait a few weeks) or  an ESP32-WROVER-B 16MB (~$5 from Mouser). A complete build should be <$40, ignoring your time. Those platforms were chosen because they have a relatively large flash (16 MiB) for storing files, which allows having documentation and support for multiple solar controllers on-board. Work is being done with the ESP32 to support SD cards for logging. There's support for monitoring and configuration via a web interface, MODBUS/TCP, and a RESTful JSON API. The UI fits well on a cell phone screen.

A core with less flash memory should work fine as long as the "data" directory will fit. Delete files from /data/doc and /data/ctl (keep the ones for your specific controller) if needed. The additional flash memory on the recommended cores is there in order to support the config and documentation files needed to support controllers beyond those currently provided in addition to future needs ("640K ought to be enough for anyone!"). Edit /data/documentation.htm if you remove documentation files, or simply live with broken links.

It is coded using the [Arduino IDE](https://www.arduino.cc/en/Main/Software), which is a bit "odd", hence code being split for clarity amongst multiple .h files. If you know how to do it better, please advise. 

The printed circuit board was done in [Eagle](https://www.autodesk.com/products/eagle/overview).

Development has been done with testing against the Prostar MPPT and Prostar Gen3 controllers, and the information/config files for those have been provided. Other controllers will need to have .csv files (see data/csv_file_desc.txt) and image files (e.g. /data/ctl/PS-MPPT.png) added. It should also be able to support Sunsavers (SSDuo, SS-MPPT), and Tristar (TS-45, TS-60, TS-MPPT-x) controllers (i.e. ones supporting MODBUS). Morningstar isn't consistent with naming or numbering the registers, so such support <s>might</s> will require more than just creating the .csv files.

MODBUS-TCP works with Morningstar's MSView, allowing it to be used to monitor and configure a controller wirelessly. To do that, in MSView, do "Devices/Manual connection", select your controller type, select Connection Type Remote, and enter the IP address or DNS name of the MStar-WLAN. OK. Then right click on the controller in the left column and "connect." MSView first downloads any logs, so may take a minute before much else happens.

![image of hardware](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/board.png)

## Getting Started

The code will run on a bare D1 Mini Pro powered by USB, but obviously can't talk to a controller so the status and config pages won't do much. But you can check it out without much effort.

To do anything useful, hardware is needed to interface with a Morningstar controller. Morningstar uses a proprietary half-duplex (open collector, see /data/img/Schematic.png) MODBUS interface. A circuit board layout is provided which includes all the necessary interfacing circuitry, even drawing power from the Morningstar controller. It also supports an optional real time clock which may be useful for future features like extended logging.

Circuit board info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with some parts info. The .zip file contains CAM files ready to send to [JLCPCB](https://jlcpcb.com/quote#/) to get boards made. Currently, 5 boards will cost your less than $10, if you will wait about a month to get them. The PCB can support either ESP core. Using a WEMOS is easier, as it uses only through-hole components. ESP32/WROVER-B requires soldering fine pitch SMDs (but provides more capability).

I have a limited number of circuit boards on hand. If you can contribute to the project, contact me and I might send you one.

### Prerequisites

Development is currently being done with Arduino IDE 1.8.10, ESP8266 Arduino platform 2.6.2 (latest git version), and ESP32 Arduino platform 1.0.4.

Library requirements are noted in the .ino file. The [ESP8266](https://github.com/esp8266/Arduino) and/or [ESP32](https://github.com/espressif/arduino-esp32) core for Arduino needs to be installed. Files may need to be removed from /data in order to upload to the available flash on an ESP32. It's also possible to modify the boards.txt and create a new (partition).csv file to allow a larger SPIFFs FS than supported by the standard boards.

The "data" directory needs to be uploaded to the board from the IDE or via OTA (Utility tab, update - but ESP8266 only, for now). The board will access the files using SPIFFS or LittleFS (ESP8266 only, for now), depending on how it's compiled. The ESP8266 binary posted here uses LittleFS, and the ESP32 one, SPIFFS. To upload, you'll need the appropriate plug-in, [SPIFFS](https://github.com/esp8266/arduino-esp8266fs-plugin) [LittleFS](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin). If you want to work with ESP32, see below, the SPIFFS and code images here are built for a custom partition table.

The data/ctl/*.csv files are used to define the different controllers. data/csv_file_desc.txt describes the format.

The WROVER-B (ESP32) build uses a custom partition scheme to provide more SPIFFS storage. The "largest_spiffs_16MB.csv" file should be put in your Arduino folder, under "hardware\espressif\esp32\tools\partitions". The "newboard.txt" file should be inserted into the "hardware\espressif\esp32\boards.txt" file (back up the original). That should result in a new "MStar-WLAN" board which you can build for, and which supports the new partition scheme.

## Operation

Using a standard "rollover" 6P6C cable (RJ connector tabs on the same side of the cable), connect to the Meterbus port of a Morningstar controller. Don't worry - it's protected if wired wrong. A 4P4C cable should also work, if it's short (less wire for power).

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air (OTA) firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP (check your DHCP server to see what address it received). You can then connect to the internal web server at that address. The firmware also informs the DCHP server with a (statistically) unique hostname of the form "MStar-WLAN-xxxxxx", where the xs are the last 3 octets of the MAC address in hex (not working with ESP32, not my fault).

OTA updates are done from the Utility tab, "Update WLAN module firmware" link. If compiled with newer ESP8266 Arduino platforms, OTA updates of the flash (/data) image are also possible. For ESP8266 firmware, the image is named "MStar-WLAN.ino.d1_mini.bin". Default is that uses the LittleFS filesystem, which is "MStar-WLAN.mklittlefs.bin". WROVER builds use "MStar-WLAN.ino.esp32.bin" and "MStar-WLAN.spiffs.bin". Currently, LittleFS is only for an ESP8266 build, and SPIFFS is for ESP32 because the partition sizes are different. There's a build #define flag to use SPIFFS with ESP8266, but why?, and you'll have to create the FS image.

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/status.png)

![image of charge settings page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/charge_settings.png)

## Support

There is none, although you can certainly ask. You'll get a better response if you can well document a bug. This was a personal project, I'm freely offering it to others for non-commercial use. Feel free to contribute back if you can (don't do github pull requests, that would just be another thing for me to figure out how to make work, I'm not a regular git user.). Things which need attention or might be an important future direction are commented with "TODO".

## General answers to any question (pick one)

Yes.

No.

That's an important future direction.

That's a significant third-party opportunity.

## More Info

More info may be found under the data directory.

## Current direction

The hardware supports one of the cheap Chinese DS3231 Real Time Clock boards. The software can be configured to connect to an NTP server, and if it does it will keep the RTC in sync (and trim its oscillator more accurately, too!). SD Card support is being added to the hardware. Both features are meant to allow better logging than what Morningstar currently provides. If someone who knows Chart.js is interested, please jump in!

## Important future directions

Possible additions: Include support for additional controllers. Allow changing user/pass for config and update. Static IP config. SMTP - email notifications for error or out-of-range conditions (or even daily updates). Logging/graphing (anyone good at jquery?). Support for simultaneous connection to multiple controllers. Regularly poll controller in background and cache the data, to speed up web page views. Async web server. Chunked responses. Provide "one click" charging settings for different batteries. Backup/restore settings. <s>Support connection to multiple, alternate SSIDs</s> (done). MQTT. SNMP. World peace.
 
## Author

* **Mike Sauve** - [mike-s123](https://github.com/mike-s123/)
Programming is not my day job, so don't be harsh about style or efficiency. Function over form. It's working for me, I hope it works for you. I formally learned on FORTRAN and PL/C. Later BASIC, assembly, and bash with a small bit of python. 98% as a hobby. OO stuff is foreign to me.

## License and copyrights

First up - if you clone/fork this, please let me know. It's not a requirement, just a request - I'd like to know who else is interested - I don't find much interest/development for similar projects.

* Original work copyright by Mike Sauve, License [CC BY-NC](https://creativecommons.org/licenses/by-nc/4.0/legalcode)

* See [LICENSES.MD](https://github.com/mike-s123/MStar-WLAN/blob/master/LICENSE.md) for more detail.

* Hardware datasheets (in the hardware directory) are copyrighted by the manufacturers. They were all pulled off public websites. I believe it is fair use to have them here, where they're doing what they were intended to do - support the associated devices. If any complain, I'll remove them.

* Morningstar documentation (in data directory) is copyrighted by Morningstar. All of it was pulled off their public website, it is here simply to make things easier. I believe it is fair use to have them here, same reason as above. If Morningstar complains, I'll remove them.