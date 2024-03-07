#! /bin/sh

#
# Copyright (c) 2024 Advanced Micro Devices, Inc.  All rights reserved.
#

EEPROM=$(/bin/ls /sys/bus/i2c/devices/1-0054/eeprom 2> /dev/null || /bin/ls /sys/bus/i2c/devices/11-0054/eeprom 2> /dev/null)
BOARD=$(/usr/sbin/ipmi-fru --fru-file="$EEPROM" --interpret-oem-data | /usr/bin/awk -F": " '/FRU Board Product/ { print tolower ($2) }')
echo Install board pacakages for "$BOARD"
dnf install -y "${BOARD}"
echo "Install complete.  Reboot required"
