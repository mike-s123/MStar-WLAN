# MStar-WLAN

This project includes both hardware and software for interfacing to a [Morningstar](https://www.morningstarcorp.com/) solar controller via wireless LAN (WiFi). The core is an ESP8266 based WEMOS D1 Mini Pro board (about $4 via AliExpress and wait a few weeks) or alternately an ESP32-WROVER-B 16MB (~$5 from Mouser). ESP32 is a work in progress (expressif hasn't done a good job with API consistency). These were chosen because they have a large flash (16 MiB) for storing files, which allows having documentation on-board. There's support for monitoring and configuration via a web interface, MODBUS/TCP, and a RESTful API.

A core with less flash memory should work fine as long as the "data" directory will upload. Delete files from /data if needed. The additional flash memory on the recommended cores is there in order to support the config and documentation files needed to support additional controllers beyond those currently provided.

It is coded using the [Arduino IDE](https://www.arduino.cc/en/Main/Software), which is a bit "odd", hence code being split for clarity amongst multiple .h files. If you know how to do it better, please advise. 

The printed circuit board was done in [Eagle](https://www.autodesk.com/products/eagle/overview).

Development has been done with testing against the Prostar MPPT and Prostar Gen3 controllers, and the information/config files for those have been provided. Other controllers will need to have .csv files (see data/csv_file_desc.txt) and image files (e.g. data/PS-MPPT.png) added.

MODBUS-TCP works with Morningstar's MSView, allowing it to be used to monitor and configure a controller wirelessly.

![image of hardware](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/board.png)

## Getting Started

The code will run on a bare D1 Mini Pro powered by USB, but obviously can't talk to a controller so the status and config pages won't do much. But you can check it out without much effort.

To do anything useful, hardware is needed to interface with a Morningstar controller. Morningstar uses a proprietary half-duplex MODBUS interface. A circuit board layout is provided which provides all the necessary interfacing circuitry, even drawing power from the Morningstar controller. It also supports an optional real time clock which may be useful for future features like extended logging.

Circuit board info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with some parts info. The PCB supports either ESP core.

### Prerequisites

Library versions are noted in the .ino file. The [ESP8266](https://github.com/esp8266/Arduino) and/or [ESP32](https://github.com/espressif/arduino-esp32) core for Arduino needs to be installed. Most development has been done with the ESP8266 platform, and that's what's working. ESP32 is still very much a work in progress. There are a lot of differences even in the "official" APIs - they don't make compatibility simple.

The "data" directory needs to be uploaded to the board ("sketch data upload"). The board will access the files using SPIFFS. The files are used to define the different controllers, and also contain some documentation.

## Operation

Using a standard "rollover" 6P6C cable, connect to the Meterbus port of a Morningstar controller. A 4P4C cable should also work, if it's short.

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP (check your server to see what address it received). You can then connect to the internal web server at that address.

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/status.png)


![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/charge_settings.png)

## Support

There is none. This was a personal project, I'm freely offering it to others for non-commercial use. Feel free to contribute back if you can. Things which need attention or might be an important future direction are commented with "TODO".

## More Info

More info may be found under the data directory.

## Important future directions

Possible additions: Include support for more controllers. Allow changing user/pass for config and update. Static IP config. SMTP - email notifications for error or out-of-range conditions (or even daily updates). Logging/graphing. MQTT. SNMP. Regularly poll controller in background and cache the data, to speed up web page views. Async web server. Provide "one click" charging settings for different batteries. Backup/restore settings. Support connection to multiple, alternate SSIDs.
 
## Author

* **Mike Sauve** - *Initial work* - [mike-s123](https://github.com/mike-s123/)
Programming is not my day job, so don't be harsh about style or efficiency.

## License and copyrights

* Original work copyright by Mike Sauve, License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode

* Some HTML/CSS stuff is from [Ian Sparkes](https://bitbucket.org/isparkes/nixiefirmwarev2/src), and is also CC-BY-NC.

* Libraries used will have their own licenses. Some parts may be subject to other licenses as noted.

* Hardware datasheets (in the hardware directory) are copyrighted by the manufacturers. They were all pulled off public websites. I believe it is fair use to have them here. If any complain, I'll remove them.

* Morningstar documentation (in data directory) is copyrighted by Morningstar. All of it was pulled off their public website, it is here simply to make things easier. I believe it is fair use to have them here. If Morningstar complains, I'll remove them.