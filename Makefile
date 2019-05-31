#
# Uninterrruptible Power Supply (UPS) for a Raspberry Pi
# Project Makefile
#
#
# This source file is part of the Raspberry Pi UPS Arduino firmware
# found under http://www.github.com/microfarad-de/pi-ups
#
# Please visit:
#   http://www.microfarad.de
#   http://www.github.com/microfarad-de
#
# Copyright (C) 2019 Karim Hraibi (khraibi at gmail.com)
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
# Notes:
#
# - This script depends on the Arduino Makefile project from:
#   https://github.com/sudar/Arduino-Makefile
#
# - The following environment variables need to be defined on your
#   host system (the actual values depend on your system configuration):
#      ARDUINO_DIR=/opt/arduino
#      ARDMK_DIR=/opt/arduino-mk
#      MONITOR_PORT=/dev/ttyUSB0



BOARD_TAG = pro
BOARD_SUB = 16MHzatmega328
MONITOR_BAUDRATE = 19200
include ${ARDMK_DIR}/Arduino.mk
