#! /bin/sh

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

SC_APP_DIR="/usr/share/system-controller-app"
CONFIG_FILE="$SC_APP_DIR/.sc_app/config"

BOARD="VN-P-B2197-00"
BOARD_JSON="$SC_APP_DIR/board/$BOARD.json"

BOARD_TEST="board_test"
BOARD_TEST_JSON="$BOARD_TEST.json"


delete_voltage()
{
    VOLTAGE=$1
    NUM_LINES=10
    BEGIN_LINE=`grep -n "\"${VOLTAGE}\" : {" $BOARD_TEST_JSON | awk '{print $1}' | sed 's/://' | tail -n 1`
    END_LINE=`expr $BEGIN_LINE + $NUM_LINES`
    sed -i "${BEGIN_LINE},${END_LINE}d" $BOARD_TEST_JSON
}

# Create a modified JSON file
cp $BOARD_JSON $BOARD_TEST_JSON

# Remove TPS53681 voltage regulators
delete_voltage VCCINT
delete_voltage VCC_CPM5N
delete_voltage VCC_IO_SOC
delete_voltage VCC_FPD

# Remove TPS546B24A voltage regulators
delete_voltage VCCO_500
delete_voltage VCCO_501
delete_voltage VCCO_502
delete_voltage VCCO_503
delete_voltage LP5_VDDQ_0V5

echo "Board: board_test" > $CONFIG_FILE
echo "Board_Path: `pwd`/" >> $CONFIG_FILE

# Stop and restart sc_appd to use the 'board_test' JSON file
systemctl stop system_controller
systemctl start system_controller
sleep 8

echo ""
echo ">>>"
echo ">>> Check Clocks"
echo ">>>"
sc_app -c BIT -t 'Check Clocks'
echo ""

echo ">>>"
echo ">>> Check Voltages"
echo ">>>"
sc_app -c BIT -t 'Check Voltages'
echo ""

# Stop and restart sc_appd to use the default JSON file
rm $CONFIG_FILE $BOARD_TEST_JSON
systemctl stop system_controller
systemctl start system_controller

