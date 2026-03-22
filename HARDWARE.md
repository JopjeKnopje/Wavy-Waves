# Hardware

this project uses a module consisting of an esp32 board with attached rtc and bmp280/'bme280 to sens air pressure differences and logs them to a central controller esp with sd card. the controller logs the data and sends the data also via rs232 to a box, where the data srteam is separated to 4 analog voltages.

BMP280 sensor, according to the datasheet it measures both pressure and temperature.
![BMP280 pressure sensor](https://europe1.discourse-cdn.com/arduino/original/4X/d/d/3/dd3f0bd7d229efd4393773ea04eecf502c4e5cba.jpeg)
DS3231 rtc module
![DS3231 rtc module](https://electropeak.com/learn/wp-content/uploads/2021/09/DS3231-1.jpg)

TODO: UPDATE README

## Sensors and Boards
- https://www.tinytronics.nl/en/sensors/air/pressure/bmp280-digital-barometer-pressure-sensor-module


