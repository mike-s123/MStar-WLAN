# MStar-WLAN

This project includes both hardware and software for interfacing to a [Morningstar](https://www.morningstarcorp.com/) solar controller via wireless LAN (WiFi). The core is an ESP8266 based WEMOS D1 Mini Pro board (about $4 via AliExpress and wait a few weeks) or maybe an ESP32-WROVER-B 16MB (~$5 from Mouser). ESP32 is definitely a work in progress (expressif hasn't done a good job with API consistency, and ESP32 doesn't have all the libraries the ESP8266 does). These were chosen because they have a relatively large flash (16 MiB) for storing files, which allows having documentation on-board. There's support for monitoring and configuration via a web interface, MODBUS/TCP, and a RESTful API.

A core with less flash memory should work fine as long as the "data" directory will fit. Delete files from /data if needed. The additional flash memory on the recommended cores is there in order to support the config and documentation files needed to support additional controllers beyond those currently provided in addition to future needs ("640K ought to be enough for anyone!"). Edit /data/documentation.htm if you remove documentation files.

It is coded using the [Arduino IDE](https://www.arduino.cc/en/Main/Software), which is a bit "odd", hence code being split for clarity amongst multiple .h files. If you know how to do it better, please advise. 

The printed circuit board was done in [Eagle](https://www.autodesk.com/products/eagle/overview).

Development has been done with testing against the Prostar MPPT and Prostar Gen3 controllers, and the information/config files for those have been provided. Other controllers will need to have .csv files (see data/csv_file_desc.txt) and image files (e.g. /data/PS-MPPT.png) added. It should also be able to support Sunsavers (SSDuo, SS-MPPT), and Tristar (TS-45, TS-60, TS-MPPT-x) controllers (i.e. ones supporting MODBUS). Morningstar isn't consistent with naming or numbering the registers, so such support might require more than just creating the .csv files.

MODBUS-TCP works with Morningstar's MSView, allowing it to be used to monitor and configure a controller wirelessly. To do that, in MSView, do "Devices/Manual connection", select your controller type, select Connection Type Remote, and enter the IP address or DNS name of the MStar-WLAN. OK. Then right click on the controller in the left column and "connect." MSView first downloads any logs, so may take a minute before much else happens.

![image of hardware](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/board.png)

## Getting Started

The code will run on a bare D1 Mini Pro powered by USB, but obviously can't talk to a controller so the status and config pages won't do much. But you can check it out without much effort.

To do anything useful, hardware is needed to interface with a Morningstar controller. Morningstar uses a proprietary half-duplex MODBUS interface. A circuit board layout is provided which includes all the necessary interfacing circuitry, even drawing power from the Morningstar controller. It also supports an optional real time clock which may be useful for future features like extended logging.

Circuit board info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with some parts info. The PCB can support either ESP core.

### Prerequisites

Development is currently being done with Arduino IDE 1.8.10, ESP8266 Arduino platform 2.6.2 (latest git version), and ESP32 Arduino platform 1.0.4.

Library requirements are noted in the .ino file. The [ESP8266](https://github.com/esp8266/Arduino) and/or [ESP32](https://github.com/espressif/arduino-esp32) core for Arduino needs to be installed. Most development has been done with the ESP8266 platform, and that's what's working. ESP32 is still very much a work in progress. There are a lot of differences even in the "official" APIs - they don't make compatibility simple.

The "data" directory needs to be uploaded to the board from the IDE ("sketch data upload"). The board will access the files using SPIFFS or LittleFS (ESP8266 only, for now), depending on how it's compiled. The binary posted here uses LittleFS. To upload, you'll need the appropriate plug-in, [SPIFFS](https://github.com/esp8266/arduino-esp8266fs-plugin) [LittleFS](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin).

The files are used to define the different controllers, and also contain some documentation.

## Operation

Using a standard "rollover" 6P6C cable, connect to the Meterbus port of a Morningstar controller. A 4P4C cable should also work, if it's short.

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air (OTA) firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP (check your server to see what address it received). You can then connect to the internal web server at that address. The firmware also informs the DCHP server with a (mostly) unique hostname of the form "MStar-WLAN-xxxxxx", where the xs are the last 3 octets of the MAC address in hex.

OTA updates are done from the Utility tab, "Update WLAN module firmware" link. With newer ESP8266 Arduino platforms, OTA updates of the flash (/data) image are also possible. For firmware, the image is named "MStar-WLAN.ino.d1_mini.bin". For LittleFS, it's "MStar-WLAN.mklittlefs.bin". 

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/status.png)

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/charge_settings.png)

## Support

There is none. This was a personal project, I'm freely offering it to others for non-commercial use. Feel free to contribute back if you can. Things which need attention or might be an important future direction are commented with "TODO".

## More Info

More info may be found under the data directory.

## Important future directions

Possible additions: Include support for additional controllers. Allow changing user/pass for config and update. Static IP config. SMTP - email notifications for error or out-of-range conditions (or even daily updates). Logging/graphing. Support for simultaneous connection to multiple controllers. Regularly poll controller in background and cache the data, to speed up web page views. Async web server. Provide "one click" charging settings for different batteries. Backup/restore settings. Support connection to multiple, alternate SSIDs. MQTT. SNMP.
 
## Author

* **Mike Sauve** - *Initial work* - [mike-s123](https://github.com/mike-s123/)
Programming is not my day job, so don't be harsh about style or efficiency.

## License and copyrights

* Original work copyright by Mike Sauve, License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode

* Some HTML/CSS stuff is from [Ian Sparkes](https://bitbucket.org/isparkes/nixiefirmwarev2/src), and is also CC-BY-NC. Thanks, Ian.

* Libraries used will have their own licenses. Some parts may be subject to other licenses, which will be noted.

* Hardware datasheets (in the hardware directory) are copyrighted by the manufacturers. They were all pulled off public websites. I believe it is fair use to have them here. If any complain, I'll remove them.

* Morningstar documentation (in data directory) is copyrighted by Morningstar. All of it was pulled off their public website, it is here simply to make things easier. I believe it is fair use to have them here. If Morningstar complains, I'll remove them.