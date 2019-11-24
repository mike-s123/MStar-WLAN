# MStar-WLAN

This project includes both hardware and software for interfacing to a Morningstar solar controller via wireless LAN (WiFi). The core is an ESP8266 based WEMOS D1 Mini Pro board (or alternately, an ESP32-WROVER-B (16MB), which is a work in progress). These were chosen because they have a large flash for storing files, which allows having documentation on-board. It supports monitoring/configuration via a web interface, MODBUS/TCP, or a RESTful API.

It is coded using the Arduino IDE, which is "odd", hence code being in .h files. The printed circuit board was done in Eagle.

Testing has been done with Prostar MPPT and Prostar Gen3 controllers. MODBUS-TCP works with Morningstar's MSView.

![image of hardware](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/board.png)

## Getting Started

Hardware info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with some parts info.

### Prerequisites

Library versions are noted in the .ino. The ESP8266 and/or ESP32 core for Arduino needs to be installed.

The "data" directory needs to be uploaded to the board ("sketch data upload"). The board will access the files using SPIFFS. The files are used to define the different controllers, and also contain some documentation.

## Operation

Using a standard "rollover" 6P6C cable, connect to the Meterbus port of a Morningstar controller. A 4P4C cable should also work, if it's short.

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP.

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/status.png)


![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/charge_settings.png)

## Support

There is none. This was a personal project, I'm freely offering it to others for non-commercial use. Feel free to contribute back if you can. Things which need attention or might be an important future direction are commented with "TODO".

## More Info

More info may be found under the data directory.
 
## Author

* **Mike Sauve** - *Initial work* - [mike-s123](https://github.com/mike-s123/)
Programming is not my day job, so don't be harsh about style or efficiency.

## License

* Original work by Mike Sauve, License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode

* Some HTML/CSS stuff is from [Ian Sparkes](https://bitbucket.org/isparkes/nixiefirmwarev2/src), and is also CC-BY-NC.

* Libraries used will have their own licenses. Some parts may be subject to other licenses as noted.
