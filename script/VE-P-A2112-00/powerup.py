#! /usr/bin/env python3

#
# Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
from periphery import I2C
import set_tps546b24a as tps546b24a

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

#
# Determine which Daughter Card is plugged-in.
#
name = read_product_name(DC_EEPROM_BUS, DC_EEPROM_ADDRESS)
#print("Daughter Card: ", name)

REGULATOR_BUS = "/dev/i2c-0"

#
# VCCO_500 is at 0x1A, VCCO_501 is at 0x1B, and VCCO_502 is at 0x1C.
#
if name == "X-PRC-07" or name == "X-PRC-08":
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1A, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1B, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1C, 3.2)
elif name == "X-PRC-09" or name == "X-PRC-10" or name == "X-PRC-11":
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1A, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1B, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1C, 1.8)
else:
    print("WARNING: unknown DC '" + name + "' is detected.  Set rails to 1.8v")
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1A, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1B, 1.8)
    tps546b24a.set_tps546b24a(REGULATOR_BUS, 0x1C, 1.8)
