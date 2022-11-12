#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
from periphery import I2C


#
# This routine gets VOUT of a TPS546B24A voltage regulator.
#
def get_tps546b24a(bus, address):
    PMBUS_VOUT_MODE = 0x20
    PMBUS_READ_VOUT = 0x8B

    i2c = I2C(bus)

    # Get the VOUT_MODE
    msgs = [I2C.Message([PMBUS_VOUT_MODE]), I2C.Message([0x0], read = True)]
    i2c.transfer(address, msgs)
    #print("VOUT_MODE: %x" % msgs[1].data[0])
    exponent = ((msgs[1].data[0] & 0x1F) - (4 * 8))

    # Read the VOUT
    msgs = [I2C.Message([PMBUS_READ_VOUT]), I2C.Message([0x00, 0x00], read = True)]
    i2c.transfer(address, msgs)
    mantissa = (msgs[1].data[1] << 8 | msgs[1].data[0])
    #print("data[0] = %x, data[1] = %x" % (msgs[1].data[0], msgs[1].data[1]))
    #print("mantissa = %x" % mantissa)

    voltage = mantissa * (2 ** exponent)

    i2c.close()

    return voltage


#
# Main routine
#

if len(sys.argv) != 2:
    print("ERROR: missing voltage target")
    quit(-1)

target = sys.argv[1]
bus = "/dev/i2c-0"
if target == "VCCO_500":
    address = 0x13
elif target == "VCCO_501":
    address = 0x10
elif target == "VCCO_502":
    address = 0x11
elif target == "VCCO_503":
    address = 0x12
elif target == "LP5_VDDQ_0V5":
    address = 0x14
else:
    print("ERROR: invalid voltage target")
    quit(-1)

voltage = get_tps546b24a(bus, address)
print("Voltage(V):\t%0.2f" % voltage)
