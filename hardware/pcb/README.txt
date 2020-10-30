Notes:
The .zip file in this folder is ready to submit to https://jlcpcb.com/ where you can get 5 boards, shipped for about US$10.

Components can be ordered through Mouser using this convenient cart: https://www.mouser.com/ProjectManager/ProjectDetail.aspx?AccessID=f1275ffe09

The other files are schematic, board layout, library and CAM processor files for Eagle CAD.

MStar-WLAN parts notes

Notes: C1 must be large, the power provided by the controller isn't always steady. The VR needs to be a switcher, a linear consumes too much power. The voltage regulator below seems to work best of those I tried, it's quite efficient in this application. As shown on the schematic, many parts are only needed if using a WROVER.

Resistors and caps aren't critical, other than below.

Most parts are standard, these are specific:
C1 or C2 - United Chemi-Con EGPD250ELL472MK35H (4700uF/25V, 12.5mm dia, 35mm long)
C12 - should be tantalum, 6.3V
D2 - Vishay SB140-E3, due to its low Vf.
VR - OKI-78SR-3.3/1.5-W36-C
VR - STM LDL1117S33R SOT-223-4 (only with WROVER)
J1 6P6C connector - Kobiconn 154-UL6661
J3 - USB C - JAE DX07S016JA3R1500 (only with WROVER)
Case - Hammond RL6105
USB/Serial - FTDI FT231XS-R SSOP-20 (only with WROVER)
Reset - Rohm IMH3AT110 SOT-457 (only with WROVER)
RTC - DS3231(ZS-042 module) Note that the header is installed on the side opposite the battery, so the battery is accessible when the module is soldered in place.
MicroSD socket - Hirose DM3BT-DSF-PEJS
TVS - BZW04-17B