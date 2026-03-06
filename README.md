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

# Building the firmware

The code checks whether its building for the Senor board or the so called Mothership™
In order to build the project for the Mothership™, just set the `WW_MOTHERSHIP` environment variable like shown below.

> [!warning]  
> Keep in mind that when changing `WW_MOTHERSHIP` between builds you have to run `idf.py clean` CMake doesn't pick up on environment changes

```bash
WW_MOTHERSHIP="" idf.py build
```


Flash the board without any montioring.
```bash
idf.py flash
```

Flash and monitor the output with a port specified.
```bash
idf.py flash -b 921600 -p /dev/ttyUSB0 monitor
```

# Dev setup
## espidf 
> [!note]  
> This is needed for actually building the firmware, the other steps are just developer tools.

Follow the steps from espressifs [instructions](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html)

I've created an alias in my `.zshrc` file to activate the espidf environment.
```zsh
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```


## LSP
Install the espressifs [`esp-clangd`](https://github.com/espressif/llvm-project) version.
```bash
idf_tools.py install esp-clang
```

We use clang instead of gcc to compile esp-idf project, for some reason this generates the correct `compile_commands.json`.

### Neo-vim plugin
https://github.com/Aietes/esp32.nvim

### compile_commands.json
If `compile_commands.json` lives in a build directory, you should symlink it to the root of your source tree.
```
ln -s build/compile_commands.json .
```


## Formatting tools
I have some pre-commits hooks setup using [prek](https://github.com/j178/prek?tab=readme-ov-file#installation) which runs [clang-formatter]().
You can install clang-format with
```bash
apt install clang-format
```

And prek using the following command, or any other of the [installation methods](https://github.com/j178/prek?tab=readme-ov-file#installation).
```bash
curl --proto '=https' --tlsv1.2 -LsSf https://github.com/j178/prek/releases/download/v0.3.4/prek-installer.sh | sh
```

To install prek into the repo run.
```bash
prek install
```

prek will run automatically on every commit you make, but you can run it manually using.
```bash
```
```
prek run --all-files
```


### Running tests locally
To set this up have a look at [this](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html#id1)


## Serial monitoring
```
minicom -b 115200 -D /dev/ttyACM0
```
