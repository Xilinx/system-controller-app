#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
from periphery import I2C


#
# This routine gets VOUT of a TPS53681 voltage regulator.
#
def get_tps53681(bus, address, channel):
    PAGE_ADDR = 0x00
    PMBUS_VOUT_MODE = 0x20
    PMBUS_READ_VOUT = 0x8B

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

    # Read the VOUT
    msgs = [I2C.Message([PMBUS_READ_VOUT]), I2C.Message([0x00, 0x00], read = True)]
    i2c.transfer(address, msgs)
    #print("data[0] = %x, data[1] = %x" % (msgs[1].data[0], msgs[1].data[1]))

    # The first step is equal to 0.25V with additional steps incremented by DAC_step volts
    voltage = (((msgs[1].data[0] - 1) * DAC_step) + 0.25)

    i2c.close()

    return voltage


#
# Main routine
#

if len(sys.argv) != 2:
    print("ERROR: missing voltage target")

target = sys.argv[1]
bus = "/dev/i2c-0"
if target == "VCCINT":
    address = 0x60
    channel = 0
elif target == "VCC_CPM5N":
    address = 0x60
    channel = 1
elif target == "VCC_IO_SOC":
    address = 0x62
    channel = 0
elif target == "VCC_FPD":
    address = 0x62
    channel = 1
else:
    print("ERROR: invalid voltage target")
    quit(-1)

voltage = get_tps53681(bus, address, channel)
print("Voltage(V):\t%0.2f" % voltage)
