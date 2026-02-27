<div align=center>

# Wavy Waves

<br />
</div>

# Hardware

this project uses a module consisting of an esp32 board with attached rtc and bmp280/'bme280 to sens air pressure differences and logs them to a central controller esp with sd card. the controller logs the data and sends the data also via rs232 to a box, where the data srteam is separated to 4 analog voltages.

BMP280 sensor
![BMP280 pressure sensor](https://europe1.discourse-cdn.com/arduino/original/4X/d/d/3/dd3f0bd7d229efd4393773ea04eecf502c4e5cba.jpeg)
DS3231 rtc module
![DS3231 rtc module](https://electropeak.com/learn/wp-content/uploads/2021/09/DS3231-1.jpg)


# Dev setup

## Python & Pip & Pytest
Install python `>3.7.x` from [python.org](https://www.python.org/downloads/release/python-31312/) or using [pyenv](https://github.com/pyenv/pyenv?tab=readme-ov-file#1-automatic-installer-recommended)

```
pip install -r requirements.txt
```

### compile_commands.json

If `compile_commands.json` lives in a build directory, you should symlink it to the root of your source tree.
```
ln -s build/compile_commands.json .
```


### Running tests locally
To set this up have a look at [this](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html#id1)


## Serial monitoring
```
minicom -b 115200 -D /dev/ttyACM0
```
