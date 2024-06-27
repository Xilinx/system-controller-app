#! /bin/bash

#
# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

SCAPP_DIR="/usr/share/system-controller-app"
SCAPP_SCRIPTDIR="${SCAPP_DIR}/script"
SCAPP_LOGDIR="${SCAPP_DIR}/.sc_app/log"

rm -rf "${SCAPP_LOGDIR}" 2> /dev/null
mkdir "${SCAPP_LOGDIR}"
BOARD=$(sc_app -c board)
SN=$(sc_app -c geteeprom -t onboard -v summary | grep 'Board Serial Number' | awk '{print $4}')
LOGNAME=log_${BOARD}_${SN}_$(date +"%d_%m_%Y-%H_%M")
LOGDIR="${SCAPP_LOGDIR}"/"${LOGNAME}"
mkdir "${LOGDIR}"

#
# Collect logs
#
cp -ar /var/volatile/log "${LOGDIR}"/volatile_log
journalctl > "${LOGDIR}"/journal.log
journalctl -u system_controller > "${LOGDIR}"/sc_appd.log
dmesg > "${LOGDIR}"/dmesg.log
ps aux > "${LOGDIR}"/pslist.log
rpm -qa > "${LOGDIR}"/installed_packages.log
"${SCAPP_SCRIPTDIR}"/version_info.sh > "${LOGDIR}"/version_info.log

for I in summary all common board multirecord; do
    sc_app -c geteeprom -t onboard -v "$I" >> "${LOGDIR}"/eeprom.log
done

if [ "${BOARD}" = "VCK190" ] || [ "${BOARD}" = "VMK180" ]; then
    dd if=/sys/bus/i2c/devices/i2c-11/11-0054/eeprom of="${LOGDIR}"/eeprom.bin bs=1 count=256 2> /dev/null
else
    dd if=/sys/bus/i2c/devices/i2c-1/1-0054/eeprom of="${LOGDIR}"/eeprom.bin bs=1 count=256 2> /dev/null
fi

#
# Create a tarfile of logs
#
cd "${SCAPP_LOGDIR}"
tar zcf "${LOGNAME}".tar.gz "${LOGNAME}"
rm -rf "${LOGNAME}"

#
# Make the tarfile visible to the GUI
#
cd /usr/share/scweb
rm -rf ./static/tmp 2> /dev/null
mkdir ./static/tmp
ln -s "${SCAPP_LOGDIR}"/"${LOGNAME}".tar.gz ./static/tmp/.
echo "./static/tmp/""${LOGNAME}"".tar.gz"
