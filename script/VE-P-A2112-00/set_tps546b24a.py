#! /usr/bin/env python3

#
# Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
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
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("ERROR: missing regulator address and voltage value")
        quit(-1)

    bus = "/dev/i2c-0"
    address = sys.argv[1]
    value = sys.argv[2]

    set_tps546b24a(bus, int(address, 16), float(value))
