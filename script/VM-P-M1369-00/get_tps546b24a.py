#! /usr/bin/env python3

#
# Copyright (c) 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
from periphery import I2C

Voltage_Rail = {
    "VCCINT": 0x10,
    "VCC_SOC": 0x11,
    "VCC_IO": 0x12,
    "VCC_PSFP": 0x13,
    "VCC_RAM": 0x14,
    "VCC_PSLP": 0x15,
    "VCCAUX": 0x16,
    "VCCAUX_PMC": 0x17,
    "VCCO_500": 0x18,
    "VCCO_501": 0x19,
    "VCCO_502": 0x1A,
    "VCCO_503": 0x1B,
    "VCC_DDR5_RDIMM_1V1": 0x1C,
    "LP5_1V0_VCCO": 0x1D,
    "VCC_FMC": 0x1E,
    "GTM_AVCC": 0x22,
    "GTM_AVTT": 0x20,
    "GTM_AVCCAUX": 0x21,
    "LP5_VCC1_1V8": 0x25,
    "LP5_VCC2_1V05": 0x26,
    "LP5_VDDQ_0V5": 0x27,
    "VCCO_HDIO": 0x29,
    "VCCINT_GT": 0x2A,
    "UTIL_1V8": 0x2B,
    "VCC_PMC": 0x2C,
    "VCC_MIPI": 0x1F
}

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
try:
    address = Voltage_Rail[target]
    voltage = get_tps546b24a(bus, address)
    print("Voltage(V):\t%0.2f" % voltage)
except KeyError:
    print("ERROR: invalid voltage target")
    quit(-1)

