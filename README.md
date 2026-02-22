<div align=center>

# Wavy Waves

<br />
</div>

## hardware

this project uses a module consisting of an esp32 board with attached rtc and bmp280/'bme280 to sens air pressure differences and logs them to a central controller esp with sd card. the controller logs the data and sends the data also via rs232 to a box, where the data srteam is separated to 4 analog voltages.




# Building
This project in built using [PlatformIO](https://platformio.org/) so install it.

## compiling the firmware
```
pio run
```


## uploading
```
pio run -t upload
```


## compile_commands.json
```
pio run -t compiledb -e uno
```

