# Hardware

Both the Sensors and the mothership get powered by a 12v powersupply.

Its a semi wireless system, the sensor modules get a 12v DC connection, the datatransfer is wireless. A second version would include a solar panel and LiPo cell.


The hardware module consists of an ESP32 board with attached BMP280 pressure sensor,differences and logs them to a central controller esp with sd card. the controller logs the data and sends the data also via RS232 to a box, where the data srteam is separated to 4 analog voltages.

## Sensor
It houses a compact perfboard housing an ESP32 dev board, a BMP280 pressure sensor and a generic 12v -> 5v buck converter.
The sensor board has 2 wires connected to the board, which are have their other ends soldered to a waterproof 4 pin connector, used for power. 
We're running 2x 12v and 2x GND, through the cable, due to it being DC.

Its all stuffed into a ~1m and ø40mm PVC pipe.
The 4 pin connector is hot-glued (or epoxied) to a cap with a rubber seal, which gets screwed onto the top of the pipe. Creating an air tight seal.


When the water level rises the air in the pipe gets compressed, which increases the pressure. We can measure this.


which is glued to a 40mm pvc cap with a rubber seal.



## Mothership
The mothership is simpler, its a perfboard with the same ESP32 dev board, buck converter 




BMP280 sensor, according to the datasheet it measures both pressure and temperature.
![BMP280 pressure sensor](https://europe1.discourse-cdn.com/arduino/original/4X/d/d/3/dd3f0bd7d229efd4393773ea04eecf502c4e5cba.jpeg)
DS3231 rtc module
![DS3231 rtc module](https://electropeak.com/learn/wp-content/uploads/2021/09/DS3231-1.jpg)

TODO: UPDATE README

## Sensors and Boards
- https://www.tinytronics.nl/en/sensors/air/pressure/bmp280-digital-barometer-pressure-sensor-module


