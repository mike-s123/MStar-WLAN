# MStar-WLAN

This project includes both hardware and software for interfacing to a Morningstar solar controller via wireless. The core is an ESP8266 based WEMOS D1 Mini Pro board (or alternately, an ESP32-WROVER-B, which is a work in progress). It supports web monitoring/configuration, MODBUS/TCP, and a RESTful API.

Testing has been done with Prostar MPPT and Prostar Gen3 controllers. MODBUS-TCP works with Morningstar's MSView.

![image of hardware](https://github.com/mike-s123/MStar-WLAN/tree/master/pics/board.png)

## Getting Started

Hardware info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with parts info.

### Prerequisites

Library versions are noted in the .ino. The ESP8266 and/or ESP32 core for Arduino needs to be installed.

The "data" directory needs to be uploaded to the board. The board will access the files using SPIFFS. The files are used to define the different controllers, and also contain some documentation.

## Operation

Using a standard "rollover" 6P6C cable, connect to the Meterbus port of a Morningstar controller. A 4P4C cable should also work, if it's short.

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP.

![image of status page](https://github.com/mike-s123/MStar-WLAN/tree/master/pics/status.png)


![image of status page](https://github.com/mike-s123/MStar-WLAN/tree/master/pics/charge_settings.png)

## Support

There is none. This was a personal project, I'm freely offering it to others. Feel free to contribute back if you can.

## More Info

More info may be found under the data directory.
 
## Contributing

Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). 

## Author

* **Mike Sauve** - *Initial work* - [mike-s123](https://github.com/mike-s123/)

## License

* Original work, License CC-BY-NC, https://creativecommons.org/licenses/by-nc/4.0/legalcode

* some HTML/CSS stuff is from [Ian Sparkes](https://bitbucket.org/isparkes/nixiefirmwarev2/src), and is also CC-BY-NC.

* libraries used will have their own licenses. some parts subject to other licenses as noted.
