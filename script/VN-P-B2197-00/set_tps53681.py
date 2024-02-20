#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
from periphery import I2C


#
# This routine sets VOUT of a TPS53681 voltage regulator.
#
def set_tps53681(bus, address, channel, voltage):
    PAGE_ADDR = 0x00
    PMBUS_OPERATION = 0x01
    PMBUS_VOUT_MODE = 0x20
    PMBUS_VOUT_COMMAND = 0x21

    i2c = I2C(bus)

    # Set the channel to access
    msgs = [I2C.Message([PAGE_ADDR]), I2C.Message([channel], read = False)]
    i2c.transfer(address, msgs)

    # Get the VOUT_MODE to determine the DAC step
    msgs = [I2C.Message([PMBUS_VOUT_MODE]), I2C.Message([0x0], read = True)]
    i2c.transfer(address, msgs)
    if (msgs[1].data[0] & 0x1F) == 0x7:
        DAC_step = 0.005
    elif (msgs[1].data[0] & 0x1F) == 0x3:
        DAC_step = 0.01
    else:
        print("ERROR: invalid VOUT_MODE value")
        quit(-1)

    # Disable VOUT
    msgs = [I2C.Message([PMBUS_OPERATION]), I2C.Message([0x0], read = False)]
    i2c.transfer(address, msgs)

    # The first step is equal to 0.25V with additional steps incremented by DAC_step volts
    VID = ((voltage - 0.25) / DAC_step) + 1
    msgs = [I2C.Message([PMBUS_VOUT_COMMAND]), I2C.Message([int(VID), 0x0], read = False)]
    i2c.transfer(address, msgs)

    # Enable VOUT
    msgs = [I2C.Message([PMBUS_OPERATION]), I2C.Message([0x80], read = False)]
    i2c.transfer(address, msgs)

    i2c.close()

    return


#
# Main routine
#

if len(sys.argv) != 3:
    print("ERROR: missing voltage target and value")
    quit(-1)

target = sys.argv[1]
value = sys.argv[2]
bus = "/dev/i2c-0"
if target == "VCCINT":
    address = 0x60
    channel = 0
elif target == "VCC_CPM5N":
    address = 0x60
    channel = 1
elif target == "VCC_IO_SOC":
    address = 0x61
    channel = 0
elif target == "VCC_FPD":
    address = 0x61
    channel = 1
else:
    print("ERROR: invalid voltage target")
    quit(-1)

set_tps53681(bus, address, channel, float(value))
