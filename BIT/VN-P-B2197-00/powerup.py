#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
from periphery import I2C


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

DIR = "/usr/share/system-controller-app/BIT/VN-P-B2197-00/"

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
    os.system(DIR + "set_tps546b24a.py VCCO_500 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_501 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_502 3.3")
elif name == "X-PRC-08":
    os.system(DIR + "set_tps546b24a.py VCCO_500 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_501 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_502 3.3")
elif name == "X-PRC-09":
    os.system(DIR + "set_tps546b24a.py VCCO_500 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_501 1.8")
    os.system(DIR + "set_tps546b24a.py VCCO_502 1.8")
else:
    print("WARNING: unsupported DC '" + name + "' is detected")
    print("WARNING: voltage regulator 'VCCO_502' is not set properly")

