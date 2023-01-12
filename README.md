# Crownstone router firmware

Crownstone router firmware runs on the ESP32 microcontroller and delivers device drivers for the Crownstone router, which can be used
to improve integration between energy consumers and energy producers.

## Features

* Wi-Fi / Ethernet
* UART (can be used for RS485 and RS232)
* Websocket connectivity / HTTP requests
* Data transport according to own Crownstone router protocol
* Async data sending / receiving using message queues and threads

## Getting started

###  Initialization

The firmware is developed using the Zephyr Project.

TODO

### Development environment

#### Regular

Follow the instructions on the Zephyr [getting started guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

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

The `crownstone-router-firmware` folder contains a `flake.nix` which is a template for all required packages and the SDK to get started with Zephyr.
Simply head over to that folder in the terminal, and run
```shell
$ nix develop
```
When running this command for the first time it may take some time before all dependecies are installed. Within the development shell,
you can start building the Crownstone router firmware.

### Building and flashing the project

With a correctly set up development environment, the Crownstone router firmware can simply be build using
```shell
$ west build -p auto -b esp32
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
