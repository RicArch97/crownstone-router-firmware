# Crownstone router firmware

Crownstone router firmware runs on the ESP32 microcontroller and delivers device drivers for the Crownstone router, which can be used
to improve integration between energy consumers and energy producers.

The firmware is developed using the Zephyr Project.

## Features

* Wi-Fi / Ethernet
* UART (can be used for RS485 and RS232)
* Websocket connectivity / HTTP requests
* Data transport according to own Crownstone router protocol
* Async data sending / receiving using message queues and threads

## Getting started

Development is done on Linux. Ubuntu or NixOS are recommended. Other distributions can also work
but the regular installation steps may be slightly different. The Nix build tool works on all distributions.

### Initialization

Clone this repository
```shell
git clone https://github.com/RicArch97/crownstone-router-firmware.git ~/crownstone-router-firmware
```

Install `west` using Python pip
```shell
pip install west
```
Or using your system's package manager if applicable.

Then run the following commands; you can change the directory if you want to
```shell
west init ~/zephyr-workspace
cd ~/zephyr-workspace
west update
west zephyr-export
```

Move the cloned repository folder into the correct place
```shell
mv ~/crownstone-router-firmware ~/zephyr-workspace/zephyr/
```

### Development environment

#### Regular

It is recommended to create a virtual environment for Python dependencies.

First install `python3-venv` using your system's package manager

Ubuntu
```shell
sudo apt install python3-venv
```

Then create a virtual environment inside the Zephyr workspace
```shell
python3 -m venv ~/zephyr-workspace/.venv
```

Activate the virtual environment, and install all Python dependencies
```shell
source ~/zephyr-workspace/.venv/bin/activate
pip install -r ~/zephyr-workspace/zephyr/scripts/requirements.txt
```
Make sure to activate the virtual environment everytime you start developing!

Then Install the Zephyr SDK according to [this guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-zephyr-sdk).

#### Nix

The Nix build tool can create reproducable build environments and development shells.
Nix can be installed on every Linux distribution with the following shell command
```shell
$ sh <(curl -L https://nixos.org/nix/install) --daemon
```
Enable support for Nix flakes by executing
```shell
$ mkdir -p ~/.config/nix
$ echo "experimental-features = nix-command flakes" > ~/.config/nix/nix.conf
```

You may have to reboot for the changes to be applied.

The `~/zephyr-workspace/zephyr/crownstone-router-firmware` folder contains a `flake.nix` which is a template for all required packages and the SDK to get started with Zephyr.
Simply head over to that folder in the terminal, and run
```shell
$ nix develop
```
When running this command for the first time it may take some time before all dependecies are installed. Within the development shell,
you can start building the Crownstone router firmware.

It is recommended to use `direnv`. This can automatically create a shell with access to environment variables and packages from `flake.nix`.
First follow [this guide](https://direnv.net/docs/installation.html) to set up direnv for your distribution and shell.

Then create the `.envrc` file for Zephyr using:
```shell
echo "use flake ./crownstone-router-firmware" > ~/zephyr-workspace/zephyr/.envrc
```

In a terminal head over to `~/zephyr-workspace/zephyr` and run
```shell
direnv allow
```
Now everytime you enter `~/zephyr-workspace/zephyr`, it will automatically open a developement shell with all dependencies and environment variables required for Zephyr development.

If you use VS Code as editor, it is recommended to install [this](https://marketplace.visualstudio.com/items?itemName=mkhl.direnv) extension to automatically load everything from `flake.nix` into your environment, which means VS Code can also use everything from the flake. This way, you don't need to have any system development packages installed.

### Building and flashing the project

With a correctly set up development environment, head over to `~/zephyr-workspace/zephyr` and run
```shell
$ west build -p auto -b esp32 crownstone-router-firmware
```
then, simply run
```shell
$ west flash
```
to flash the firmware on an ESP32. The firmware is specfically made for and tested on
Espressif ESP32-WROOM-32E.

### Debugging

To monitor serial output over a UART connection, run
```shell
$ west espressif monitor
```
