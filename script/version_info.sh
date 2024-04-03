#! /bin/sh

#
# Copyright (c) 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

print_msg() {
    echo ""
    echo "$1"
    LEN=$(echo "$1" | wc -L)
    for ((i = 0; i < "${LEN}"; i++)); do
        echo -n "="
    done
    echo ""
}

if [ -f /etc/profile.d/socks_proxy.sh ]; then
    . /etc/profile.d/socks_proxy.sh
fi

EEPROM=$(/bin/ls /sys/bus/i2c/devices/1-0054/eeprom 2> /dev/null || /bin/ls /sys/bus/i2c/devices/11-0054/eeprom 2> /dev/null)
BOARD=$(/usr/sbin/ipmi-fru --fru-file="$EEPROM" --interpret-oem-data | /usr/bin/awk -F": " '/FRU Board Product/ { print $2 }')
LEGACY_BOARD=$(if [ "${BOARD}" = "VCK190" ] || [ "${BOARD}" = "VMK180" ]; then echo "1"; else echo "0"; fi)

# Information about QSPI image
if [ "${LEGACY_BOARD}" -eq 0 ]; then
    MSG="QSPI Image Information"
    print_msg "${MSG}"
    /usr/bin/image_update -p
fi

# Information about the image on boot device
if [ "${LEGACY_BOARD}" -eq 0 ]; then
    BOOT_DEVICE="eMMC"
else
    BOOT_DEVICE="SD"
fi

MSG="${BOOT_DEVICE} Image Information"
print_msg "${MSG}"
cat /etc/os-release

# Information about individual packages
RPMS="system-controller-app scweb acap labtool-jtag-support pmtool raft power-advantage-tool"
RPM_INFO=$(dnf info -C $RPMS 2>/dev/null)

for I in ${RPMS}; do
    MSG="Package Information for '$I'"
    VERS=$(echo "$RPM_INFO" | grep -A 2 -e "Name.*: $I" | grep 'Release')
    print_msg "${MSG}"
    echo -n "Installed "
    echo "$VERS" | head -n 1

    if [ "$(echo "$VERS" | wc -l)" -ge 2 ]; then
        echo -n "Latest    "
        echo "$VERS" | tail -n 1
    fi
done
