#!/usr/bin/env python
#
# Uninterruptible Power Supply (UPS) Control
#
# This source file is part of the follwoing repository:
# http://www.github.com/microfarad-de/pi-ups
#
# Please visit:
#   http://www.microfarad.de
#   http://www.github.com/microfarad-de
#
# Copyright (C) 2019 Karim Hraibi (khraibi@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
# Note:
#   User must have sudo rights to be able to perform a system shutdown
#

import serial  # pip install pyserial
import ilock   # pip install ilock
import sys
import time
import os


# Serial device
DEVICE = "/dev/ttyUSB0"

# Serial port baud rate
BAUD_RATE = 19200

# Polling interval in seconds
INTERVAL = 5




# Print info log message
def infoLog(text):
    print(text.rstrip())


# Print warning log message
def warningLog(text):
    print("[WARNING] " + text.rstrip())


# Print error log message
def errorLog (text):
    print("[ERROR] " + text.rstrip())


# Read the contents of the receive buffer
def read():
    rx = " "
    result = ""
    while len(rx) > 0:
        rx = ser.readline()
        result = result + rx
    time.sleep(0.1)
    return result


# Write to the transmit buffer
def write(str):
    ser.write(str)
    time.sleep(0.1)







#################
####  START  ####
#################


# System-wide lock ensures mutually exclusive access to the serial port
lock = ilock.ILock(DEVICE, timeout=600)

infoLog("UPS service started")

# Initialize the serial port
with lock: # Ensure exclusive access through system-wide lock
    ser = serial.Serial(DEVICE, BAUD_RATE, timeout=0.1)


# A serial connection will cause the MCU to reboot
# The following will flush the initial boot message
# and wait until the MCU is up and running
time.sleep(2)
with lock:
    result = read()
sys.stdout.write(result)
time.sleep(2)


lastResult = ""
lastMeasResult = ""
measCount = 0

# Main loop
while 1:

    # Read the UPS status
    with lock:
        write("stat\n")
        result = read()

    # Trace the UPS status
    if result != lastResult:
        lastResult = result
        if "BATTERY" in result:
            warningLog(result)
        elif "ERROR" in result:
            errorLog(result)
        else:
            infoLog(result)

    # Handle low battery condition
    if "BATTERY 0" in result:
        with lock:
            write("halt\n")
            result = read()
        if "SHUTDOWN" in result:
            errorLog(result)
            os.popen("sudo halt")
        else:
            errorLog("shutdown failed")

    time.sleep(INTERVAL)


