#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
from periphery import I2C


#
# This routine sets VOUT of a TPS546B24A voltage regulator.
#
def set_tps546b24a(bus, address, voltage):
    PMBUS_OPERATION = 0x1
    PMBUS_VOUT_MODE = 0x20
    PMBUS_VOUT_COMMAND = 0x21
    PMBUS_READ_VOUT = 0x8B

    i2c = I2C(bus)

    # Read VOUT_MODE
    msgs = [I2C.Message([PMBUS_VOUT_MODE]), I2C.Message([0x00], read = True)]
    i2c.transfer(address, msgs)
    #print("VOUT_MODE: %02x" % msgs[1].data[0])
    exponent = (msgs[1].data[0] - (4 * 8))

    # Disable VOUT
    msgs = [I2C.Message([PMBUS_OPERATION]), I2C.Message([0x00], read = False)]
    i2c.transfer(address, msgs)

    vout = round(voltage / (2 ** exponent))
    #print("VOUT: %x" % vout)

    # Set VOUT
    msgs = [I2C.Message([PMBUS_VOUT_COMMAND]), I2C.Message([(vout & 0xFF), (vout >> 8)], read = False)]
    i2c.transfer(address, msgs)

    # Enable VOUT
    msgs = [I2C.Message([PMBUS_OPERATION]), I2C.Message([0x80], read = False)]
    i2c.transfer(address, msgs)

    i2c.close()
    return


#
# This routine reads 'Product Name' field of an EEPROM.
#
def read_product_name(bus, address):
    PRODUCT_NAME_LEN = 16
    PRODUCT_NAME_OFFSET = 0x16

    i2c = I2C(bus)

    # Create an array to hold 'Product Name'
    product_name = [0 for i in range(PRODUCT_NAME_LEN)] 

    # Create message to set the reference address
    msg_ref_address = I2C.Message([PRODUCT_NAME_OFFSET], read = False)

    # Create message to read data
    msg_read = I2C.Message(product_name, read = True)

    # Assemble messages
    msgs = [msg_ref_address, msg_read]

    # Call transfer function
    i2c.transfer(address, msgs)

    i2c.close()
    return ''.join(map(chr, msgs[1].data))[:8]


#
# Main routine
#

DC_EEPROM_BUS = "/dev/i2c-1"
DC_EEPROM_ADDRESS = 0x52

REGULATOR_BUS = "/dev/i2c-0"
VCCO_500_ADDRESS = 0x13
VCCO_501_ADDRESS = 0x10
VCCO_502_ADDRESS = 0x11
VCCO_503_ADDRESS = 0x12

#
# Determine which Daughter Card is plugged-in.
#
name = read_product_name(DC_EEPROM_BUS, DC_EEPROM_ADDRESS)
#print("Daughter Card: ", name)

if name == "X-PRC-07":
    set_tps546b24a(REGULATOR_BUS, VCCO_500_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_501_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_502_ADDRESS, 3.3)
elif name == "X-PRC-08":
    set_tps546b24a(REGULATOR_BUS, VCCO_500_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_501_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_502_ADDRESS, 3.3)
elif name == "X-PRC-09":
    set_tps546b24a(REGULATOR_BUS, VCCO_500_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_501_ADDRESS, 1.8)
    set_tps546b24a(REGULATOR_BUS, VCCO_502_ADDRESS, 1.8)
else:
    print("ERROR: unsupported DC '" + name + "' is detected")
    quit(-1)

