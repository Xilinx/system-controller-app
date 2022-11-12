#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
from periphery import I2C


#
# This routine sets VOUT of a TPS546B24A voltage regulator.
#
def set_tps546b24a(bus, address, voltage):
    PMBUS_OPERATION = 0x01
    PMBUS_VOUT_MODE = 0x20
    PMBUS_VOUT_COMMAND = 0x21

    i2c = I2C(bus)

    # Get the VOUT_MODE
    msgs = [I2C.Message([PMBUS_VOUT_MODE]), I2C.Message([0x0], read = True)]
    i2c.transfer(address, msgs)
    #print("VOUT_MODE: %x" % msgs[1].data[0])
    exponent = ((msgs[1].data[0] & 0x1F) - (4 * 8))
    #print("Exponent: %x" % exponent)

    # Disable VOUT
    msgs = [I2C.Message([PMBUS_OPERATION]), I2C.Message([0x0], read = False)]
    i2c.transfer(address, msgs)

    # Set VOUT
    vout = round(voltage / (2 ** exponent))
    #print("VOUT_COMMAND: %x" % vout)
    msgs = [I2C.Message([PMBUS_VOUT_COMMAND]), I2C.Message([(vout & 0xFF), (vout >> 8)], read = False)]
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

set_tps546b24a(bus, address, float(value))
