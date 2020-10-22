# MStar-WLAN

This project includes both software and hardware design for interfacing to a [Morningstar](https://www.morningstarcorp.com/) solar controller via wireless LAN (WiFi). It's 99.9% working, with features still being added. The core is an ESP32-WROVER-B 16MB (~$5 from Mouser) or an ESP32 DevkitC v4. A complete build should be <$40, ignoring your time. Those platforms were chosen because they're available with a relatively large flash (16 MiB) for storing files, which allows having documentation and support for multiple solar controllers on-board. ESP32 supports MS-View compatible logging of controller info to an SD card, in addition to a debugging log. There's support for monitoring and configuration via a web interface, MODBUS/TCP, and a RESTful JSON API. The UI fits well on a cell phone screen.

A core with less flash memory should work fine as long as the "data" directory will fit. Delete files from /data/doc and /data/ctl (keep the ones for your specific controller) if needed. The additional flash memory on the recommended cores is there in order to support the config and documentation files needed to support controllers beyond those currently provided in addition to future needs ("640K ought to be enough for anyone!"). Edit /data/documentation.htm if you remove documentation files, or simply live with broken links. Or move the files to the SD card and change the code.

It is written using the [Arduino IDE](https://www.arduino.cc/en/Main/Software), which is a bit "odd", hence code being split for clarity amongst multiple .h files. If you know how to do it better, please advise. Yes, I know some of the code is butt-ugly. But it works. Function over form.

The printed circuit board was done in [Eagle](https://www.autodesk.com/products/eagle/overview).

Development has been done testing against the Prostar MPPT and Prostar PWM Gen3 controllers, and the information/config files for those have been provided. Other controllers will need to have register definition .csv files (see data/csv_file_desc.txt) added. Might need to do some work for register datatypes - Morningstar has many (really, what were they thinking creating one which based on a 16 bit int, n·316.67·2^-15, and multiple similar) and they've changed over time. It should also be able to support Sunsavers (SSDuo, SS-MPPT), and Tristar (TS-45, TS-60, TS-MPPT-x) controllers (i.e. ones supporting MODBUS). Morningstar isn't consistent with naming or numbering the registers, so such support will require more than just creating the .csv files.

MODBUS-TCP works with Morningstar's MSView, allowing it to be used to monitor and configure a controller wirelessly. To do that, in MSView, do "Devices/Manual connection", select your controller type, select Connection Type Remote, and enter the IP address or DNS name of the MStar-WLAN. OK. Then right click on the controller in the left column and "connect." MSView first downloads any logs, so may take a minute before much else happens. That should work with any controller supporting MODBUS, it doesn't depend on the .csv files, so if nothing else, it can be a wireless alternative to their USB/RS-232 to MeterBus converters (except for doing firmware updates).

![image of hardware](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/board.png)

## Getting Started

The code will run on a bare ESP board powered by USB, but obviously can't talk to a controller so the status and config pages won't do much. But you can check it out without much effort.

To do anything useful, hardware is needed to interface with a Morningstar controller. Morningstar uses a proprietary half-duplex (open collector, see /data/img/Schematic.png) MODBUS interface. A circuit board layout is provided which includes all the necessary interfacing circuitry, even drawing power from the Morningstar controller. It also supports an optional real time clock to support logging. It supports ntp, so even without an RTC can timestamp logs if it has a WLAN connection to the Internet.

Circuit board info is found in the "hardware/pcb" directory, where there are Eagle .brd, .sch and .lbr files, along with some parts info. The .zip file contains CAM files ready to send to [JLCPCB](https://jlcpcb.com/quote#/) to get boards made. Currently, 5 boards will cost your less than $10, if you will wait about a month to get them. The PCB was made to allow either all "through hole" components (using a DevkitC), or SMT ones (using a WROVER). If you use a DevkitC, note that 16 MiB ones aren't easy to find, so you may have to trim stuff as mentioned above, or desolder the module it came with an solder on a WROVER (16 MiB), which is still much easier than doing a full SMT build.

I have a limited number of circuit boards on hand. If you can contribute to the project, contact me and I might send you one.

### Prerequisites

Development is currently being done with Arduino IDE 1.8.10, and ESP32 Arduino platform 1.0.4.

Library requirements are noted in the .ino file. The [ESP32](https://github.com/espressif/arduino-esp32) core for Arduino needs to be installed. Modified boards.txt (newboards.txt) and partition (largest_spiffs_16MB.csv) files are provided to better support large SPIFFs.

The "data" directory needs to be uploaded to the board from the IDE or via OTA (Utility tab, update). The board will access the files using SPIFFS. After installing the code via the IDE, you can upload a SPIFFS image via OTA. Or, you can use the [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) to uploade from the IDE.

The ESP32 builds use a custom partition scheme to provide more SPIFFS storage. The "largest_spiffs_16MB.csv" file should be put in your Arduino folder, under "hardware\espressif\esp32\tools\partitions". The "boards.txt" file should be put into the "hardware\espressif\esp32\boards.txt" file (back up the original first). That should result in a new "MStar-WLAN" board which you can build for, and which supports the new partition scheme.

The data/ctl/*.csv files are used to define the different controllers. data/csv_file_desc.txt describes the format.


## Operation

Using a standard "rollover" (tabs on the same side of the cable) 6P6C ("RJ12") cable, connect to the Meterbus port of a Morningstar controller. Don't worry - it's protected if wired wrong. A 4P4C cable should also work, if it's short (fewer wires for power).

The MStar-WLAN will initially come up as a wireless access point with SSID "MStar" and PSK of "morningstar". It will have an address of 192.168.4.1. 

The web interface has some minimal security. Any page which allows configuration changes is protected with username "admin", password "setup". To do an Over-The-Air (OTA) firmware update, the username is "admin", password "update".

Connecting to http://192.168.4.1/ will display the main status page. Other pages are shown along the top. "Utility/Wireless settings" will allow you to connect to a wireless network, from which the MStar-WLAN will get an address via DHCP (check your DHCP server to see what address it received). You can then connect to the internal web server at that address. The firmware also informs the DCHP server with a (statistically) unique hostname of the form "MStar-WLAN-xxxxxx", where the xs are the last 3 octets of the MAC address in hex (not working with ESP32, not my fault).

OTA updates are done from the Utility tab, "Update WLAN module firmware" link. OTA updates of the flash (/data) image are also possible. ESP32 uses "MStar-WLAN.ino.esp32.bin" and "MStar-WLAN.spiffs.bin". 

![image of status page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/status.png)

![image of charge settings page](https://raw.githubusercontent.com/mike-s123/MStar-WLAN/master/pics/charge_settings.png)

## Support

There is none, although you can certainly ask - if you're having a problem, you've probably found something which I'll eventually run into. You'll get a better response if you can well document a bug. This was a personal project, I'm freely offering it to others for non-commercial use. Feel free to contribute back if you can (don't do github pull requests, that would just be another thing for me to figure out how to make work, I'm not a regular git user.). Things which need attention or might be an important future direction are commented with "TODO".

## General answers to any question (pick one)

Yes.

No.

That's an important future direction.

That's a significant third-party opportunity.

## More Info

More info may be found under the data directory.

## Current work

The hardware supports one of the cheap Chinese DS3231 Real Time Clock boards. The software can be configured to connect to an NTP server, and if it does it will keep the RTC in sync (and trim its oscillator more accurately, too!). Either a good network connection is needed (for NTP), or an RTC, for accurate logging. Logging to SD Card is working. The status page now updates dynamically. If someone who knows Chart.js is interested, please jump in!

## Important future directions

Possible additions: Include support for additional controllers (if someone has a broken one which might still talk MODBUS, feel free to donate). <s>Allow changing user/pass for config and update.</s> Static IP config. SMTP - email notifications for error or out-of-range conditions (or even daily updates). <s>Logging/</s>graphing (anyone good at jquery?). Support for simultaneous connection to multiple controllers. <s>Async web server.</s> Chunked responses. Provide "one click" charging settings for different batteries. Backup/restore settings. <s>Support connection to multiple, alternate SSIDs</s> (done). MQTT. SNMP. Serial passthrough, to allow a direct MS-View connection and firmware upgrades via USB. Move large files to SD card. ???.
 
## Author

* **Mike Sauve** - [mike-s123](https://github.com/mike-s123/)
Programming is not my day job, so don't be harsh about style or efficiency. Function over form. It's working for me, I hope it works for you. I formally learned on FORTRAN and PL/C. Later BASIC, assembly, and bash with a small bit of python. 98% as a hobby. OO stuff is foreign to me. 

## License and copyrights

First up - if you clone/fork this, please let me know. It's not a requirement, just a request - I'd like to know who else is interested - I haven't found much interest/development for similar projects.

* Original work copyright by Mike Sauve, License [CC BY-NC](https://creativecommons.org/licenses/by-nc/4.0/legalcode)

* See [LICENSES.MD](https://github.com/mike-s123/MStar-WLAN/blob/master/LICENSE.md) for more detail.

* Hardware datasheets (in the hardware directory) are copyrighted by the manufacturers. They were all pulled off public websites. I believe it is fair use to have them here, where they're doing what they were intended to do - support the associated devices. If any complain, I'll remove them.

* Morningstar documentation (in data directory) is copyrighted by Morningstar. All of it was pulled off their public website, it is here simply to make things easier. I believe it is fair use to have them here, same reason as above. If Morningstar complains, I'll remove them.