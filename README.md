<div align=center>

# Wavy Waves
ESP-NOW Sensor network for capturing water wave levels, using a BMP280 pressure sensor enclosed in a pipe. This data gets send to the ["Mothership"](HARDWARE.md), which uses a MCP4725 to generate a [control voltage](https://en.wikipedia.org/wiki/CV/gate#CV)

<br />
</div>

## About
This repo contains the code for both the ["Mothership"](mothership) and the ["Sensor"](sensor), both are built using [ESPIDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/get-started/index.html) 

For the hardware take a look at [HARDWARE.md](HARDWARE.md)

## TODO
- [ ] Proper logging system instead of just spamming the console with messages.
- [ ] Serial interface to setup the upper and lower bound for the pressure sensors.

## Building the firmware
Move into the directory of the device you want to build for, either.
```bash
cd sensor
# or
cd mothership
```
Setup the environment variables, and get ESP-IDF in your PATH.
```bash
source ../set-env.sh
```
### Building / Flashing
Build the actual firmware
```
idf.py build

```

Flash the firmware
```
idf.py flash
# or with a specific port
idf.py flash -p /dev/XXX
```

> [!TIP]
>  You can combine the commands above comands
>```bash
> idf.py -p /dev/ttyUSB0 build flash monitor
>```


## Dev Toolchain
This bit will go over the additional tooling used in this project.
### ESPIDF 
Follow the steps from espressifs [instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/get-started/index.html)

I've created an alias in my `.zshrc` file to activate the espidf environment.
```zsh
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```


### LSP
Install the espressifs [`esp-clangd`](https://github.com/espressif/llvm-project) version.
```bash
idf_tools.py install esp-clang
```

We use clang instead of gcc to compile esp-idf project, for some reason this generates the correct `compile_commands.json`.

#### Neo-vim plugin
https://github.com/Aietes/esp32.nvim

#### compile_commands.json
If `compile_commands.json` lives in a build directory, you should symlink it to the root of your source tree.
```
ln -s build/compile_commands.json .
```


#### Formatting tools
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
```
prek run --all-files
```



## Serial monitoring
```
minicom -b 115200 -D /dev/ttyACM0
```
