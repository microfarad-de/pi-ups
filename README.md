# Raspberry Pi (UPS)

This repository contains the Arduino firmware implementation of an Uninterruptible Power Supply (UPS) device suitable for a Raspberry Pi or any similar single-board PC running on 5 Volts with less than 3 Amperes of power consumption. The code has been implemented and tested on an _Arduino Pro Mini_ clone board based on the _ATmega328P_ running at 16MHz. Other Arduino models would work as well.

Please visit www.microfarad.de/pi-ups for a full description of this project.

Unless stated otherwise within the source file headers, please feel free to use and distribute this code under the *GNU General Public License v3.0*.

*Disclaimer: this projects implements a software based Lithium-Ion battery charger. Overcharging, short-circuiting or otherwise abusing Lithium-Ion batteries may result in a fire and/or a violent explosion. The author of this code neither takes any responsibility nor can be held liable for any damage caused to human beings and things due to the improper handling of Lithium-Ion batteries. Please implement this project at your own risk!*

## Prerequisites

* ATmega328P based Arduino Pro Mini, Arduino Nano or similar model
* Custom boot loader from: https://github.com/microfarad-de/boot-loader

## Circuit Diagram

The circuit diagram for this device can be found under the */doc* folder or can be downloaded using the follwoing link:
https://github.com/microfarad-de/pi-ups/raw/master/doc/pi-ups-schematic.pdf
