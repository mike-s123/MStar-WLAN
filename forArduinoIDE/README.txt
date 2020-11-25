Board and partition info.
-------------------------

This can be added directly into the espressif/esp32 hierachy, or via symlinks.

MStar-WLAN_boards.txt is the board definition file.
The .csv files contain partition info for 16, 8, and 4 MB flash models.

===========
Adding to espressif/esp32. Board will be at bottom of list.

MStar-WLAN_boards.txt gets appended to ~/Documents/Arduino/hardware/espressif/esp32/boards.txt. The board will appear at the very end in the Arduino IDE.

The .csv files get copied to ~/Documents/Arduino/hardware/espressif/esp32/tools/partitions

=============
Using symlinks (assumes Git Bash available). Board will be listed in it's own section.

#!/bin/bash

CURDIR=$PWD
cd ~/Documents/Arduino/hardware/
rm -r MStar-WLAN
cd espressif/esp32
git pull -v
cd ~/Documents/Arduino/hardware/espressif/esp32/tools
./get.exe
cd ~/Documents/Arduino/hardware/
mkdir MStar-WLAN
mkdir MStar-WLAN/esp32
cd MStar-WLAN/esp32
ln -s ../../espressif/esp32/* .
rm boards.txt
rm platform.txt
cp ../../../MStar-WLAN/forArduinoIDE/MStar-WLAN_boards.txt ./boards.txt
cp ../../../MStar-WLAN/forArduinoIDE/MStar-WLAN_platform.txt ./platform.txt
cp ../../../MStar-WLAN/forArduinoIDE/*.csv ./tools/partitions
cp ../../../MStar-WLAN/forArduinoIDE/esptool.exe ./tools/esptool/esptool.exe
cd $CURDIR
