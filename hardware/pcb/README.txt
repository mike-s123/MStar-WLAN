Notes:
The .zip file in this folder is ready to submit to https://jlcpcb.com/ where you can get 5 boards, shipped for about US$10.

Components can be ordered through Mouser using this convenient cart: https://www.mouser.com/ProjectManager/ProjectDetail.aspx?AccessID=f1275ffe09

The other files are schematic, board layout, library and CAM processor files for Eagle CAD.

MStar-WLAN parts notes

Notes: C1 should be large, C12 low ESR. The power provided by the controller isn't always steady. The VR needs to be a switcher, a linear consumes too much power. The voltage regulator below seems to work best of those I tried, it's quite efficient in this application. As shown on the schematic, many parts are only needed if using a WROVER.

Resistors and caps aren't critical, other than below. Note that the resistor footprints are for miniature (3.4mm) ones.

Most parts are standard, these are somewhat specific:
C1 - Panasonic EEU-FR1V222L (2200 uF/35 VDC)
C12 - should be low ESR (Panasonic 	EEU-FS0J202LB 2000 uF/6.3 VDC)
D2 - Vishay SB140-E3, due to its low Vf.
VR1 - OKI-78SR-3.3/1.5-W36-C, supports large capacitive loads
VR2 - STM LDL1117S33R SOT-223-4 (only with WROVER)
J1 - 6P6C connector - Kycon GLX-N-66
J2 - SD Card - Hirose DM3BT-DSF-PEJS Note the contacts are away from the board.
J3 - USB C - JAE DX07S016JA3 This is the most difficult to hand solder SMD on the board. (only with WROVER)
Case - Hammond RL6105, board mounts to cover
USB/Serial - FTDI FT231XS-R SSOP-20 (only with WROVER) - to be correct, use ftprog (https://www.ftdichip.com/Support/Utilities.htm#FT_PROG) to program the chip with the FT231X.xml template, which tells the host to provide 500 mA to power the board when connected to a USB 2.1 host.
Reset - Rohm IMH3AT110 SOT-457 (only with WROVER)
TVS - BZW04-33 33V unidirectional

RTC - Not included in the Mouser project BOM. DS3231(common ZS-042 module, search Amazon for DS3231) Note that the header is installed on the side opposite the battery, so the battery is accessible when the module is in place. The module also needs to be modified so it doesn't try to charge the battery (remove the LED while you're at it). https://www.onetransistor.eu/2019/07/zs042-ds3231-battery-charging-circuit.html

- potential future for on-board RRD logging - FRAM Cypress CY15B108QN-40SXI (expensive, but 10^15 cycles) Fujitsu ‎MB85AS8MTPF-G-KBERE1‎ (not FRAM, ReRAM, but 1000000 cycles) Unfortunately, sharing SPI with the SD Card currently has issues.