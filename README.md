# Raspberry Pi Uninterruptible Power Supply

This repository contains the Arduino firmware implementation of an uninterruptible power supply (UPS) device suitable for a Raspberry Pi or any similar single-board computer running on 5 Volts with a maximum of 2.5 Amperes of current consumption. The code has been implemented and tested on an _Arduino Pro Mini_ clone board based on the _ATmega328P_ running at 16MHz. Other Arduino models would work as well.

This software includes the Arduino firmware running on the UPS device as well as the Python script `ups.py` which runs on the Raspberry Pi. The Python script monitors the UPS by periodically polling its status over the serial port. The script takes care of event logging and ensures a proper system shutdown once a low battery event has been detected.

This project uses Git submodules. In order to get its full source code, please clone this Git repository to your local workspace, then execute the follwoing command from within the repository's root directory: `git submodule update --init`.

Please visit http://www.microfarad.de/pi-ups for a full description of this project.

Unless stated otherwise within the source file headers, please feel free to use and distribute this code under the *GNU General Public License v3.0*.

*Disclaimer: this project implements a software based Lithium-Ion battery charger. Overcharging, short-circuiting or otherwise abusing Lithium-Ion batteries may result in a fire and/or a violent explosion. The author of this code neither takes any responsibility nor can be held liable for any damage caused to human beings and things due to the improper handling of Lithium-Ion batteries. Please implement this project at your own risk!*

## Prerequisites

* ATmega328P based Arduino Pro Mini, Arduino Nano or similar model
* Custom bootloader from: https://github.com/microfarad-de/bootloader

## Circuit Diagram

The circuit diagram for this device can be found under the */doc* folder or can be downloaded using the follwoing link:
https://github.com/microfarad-de/pi-ups/raw/master/doc/pi-ups-schematic.pdf
