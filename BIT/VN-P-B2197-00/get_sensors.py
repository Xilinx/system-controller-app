#! /usr/bin/env python3

#
# Copyright (c) 2022 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
import sys
import subprocess


regulators = {
    'VCCINT': 'tps53681-i2c-0-60',
    'VCC_CPM5N': 'tps53681-i2c-0-60',
    'VCC_IO_SOC': 'tps53681-i2c-0-61',
    'VCC_FPD': 'tps53681-i2c-0-61',
    'VCC_RAM': 'tps544b25-i2c-0-0a',
    'VCC_LPD': 'tps544b25-i2c-0-0b',
    'VCCAUX': 'tps544b25-i2c-0-1a',
    'VCCAUX_LPD': 'tps544b25-i2c-0-0d',
    'VCCO_700': 'tps544b25-i2c-0-16',
    'VCCO_706': 'tps544b25-i2c-0-17',
    'GTYP_AVCC': 'tps544b25-i2c-0-20',
    'GTYP_AVTT': 'tps544b25-i2c-0-21',
    'GTYP_AVCCAUX': 'tps544b25-i2c-0-22',
    'GTM_AVCC': 'tps544b25-i2c-0-23',
    'GTM_AVTT': 'tps544b25-i2c-0-24',
    'GTM_AVCCAUX': 'tps544b25-i2c-0-25',
    'LP5_VDD1_1V8': 'tps544b25-i2c-0-0e',
    'LP5_VDD2_1V05': 'tps544b25-i2c-0-0f',
    'UTIL_1V8': 'tps544b25-i2c-0-15',
}

ina226s = {
    'VCC_RAM': 'ina226_u1700-isa-0000',
    'VCC_LPD': 'ina226_u1732-isa-0000',
    'VCCAUX': 'ina226_u1733-isa-0000',
    'VCCAUX_LPD': 'ina226_u1736-isa-0000',
    'VCCO_500': 'ina226_u1737-isa-0000',
    'VCCO_501': 'ina226_u1739-isa-0000',
    'VCCO_502': 'ina226_u1741-isa-0000',
    'VCCO_503': 'ina226_u1743-isa-0000',
    'VCCO_700': 'ina226_u1745-isa-0000',
    'VCCO_706': 'ina226_u1747-isa-0000',
    'GTYP_AVCC': 'ina226_u1750-isa-0000',
    'GTYP_AVTT': 'ina226_u1752-isa-0000',
    'GTYP_AVCCAUX': 'ina226_u1754-isa-0000',
    'GTM_AVCC': 'ina226_u1756-isa-0000',
    'GTM_AVTT': 'ina226_u1758-isa-0000',
    'GTM_AVCCAUX': 'ina226_u1760-isa-0000'
}

#
# Main routine
#

if len(sys.argv) != 2:
    print("ERROR: missing voltage target")
    quit(-1)

target = sys.argv[1]

if (target in regulators and target in ina226s):
    proc = subprocess.Popen(['/usr/bin/sensors', regulators.get(target), ina226s.get(target)], \
                            stdout=subprocess.PIPE)
elif (target in regulators):
    proc = subprocess.Popen(['/usr/bin/sensors', regulators.get(target)], stdout=subprocess.PIPE)
elif (target in ina226s):
    proc = subprocess.Popen(['/usr/bin/sensors', ina226s.get(target)], stdout=subprocess.PIPE)
else:
    print("ERROR: no information is available for '" + target + "'")
    quit(-1)

total_current = 0

while True:
    line = proc.stdout.readline().decode('ASCII')
    if not line:
        break

    # Voltage
    if (target == 'VCC_CPM5N' or target == 'VCC_FPD'):
        out = 'vout2:'
    else:
        out = 'vout1:'

    if line.find(out) > -1:
        tokens = line.split()
        if tokens[2] == 'mV':
            voltage = float(tokens[1]) / 1000.0
        else:
            voltage = float(tokens[1])

    # Current
    if (target == 'VCCINT' or target == 'VCC_IO_SOC'):
        out = 'iout1.'
    elif (target == 'VCC_CPM5N' or target == 'VCC_FPD'):
        out = 'iout2:'
    else:
        out = 'iout1:'

    if line.find(out) > -1:
        tokens = line.split()
        if tokens[2] == 'mA':
            current = float(tokens[1]) / 1000.0
        else:
            current = float(tokens[1])

        if (target == 'VCCINT' or target == 'VCC_IO_SOC'):
            total_current += current

    # Power
    if (target == 'VCCINT' or target == 'VCC_IO_SOC'):
        out = 'pout1:'
    elif (target == 'VCC_CPM5N' or target == 'VCC_FPD'):
        out = 'pout2:'
    else:
        out = 'power1:'

    if line.find(out) > -1:
        tokens = line.split()
        if tokens[2] == 'mW':
            wattage = float(tokens[1]) / 1000.0
        else:
            wattage = float(tokens[1])


if (target in regulators):
    print('Voltage(V):\t%.4f' % voltage)
    if (target == 'VCCINT' or target == 'VCC_IO_SOC'):
        print('Current(A):\t%.4f' % total_current)
    else:
        print('Current(A):\t%.4f' % current)

if (target in ina226s or target == 'VCCINT' or target == 'VCC_CPM5N' or \
    target == 'VCC_IO_SOC' or target == 'VCC_FPD'):
    print('Power(W):\t%.4f' % wattage)

