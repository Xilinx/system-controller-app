#! /usr/bin/env python3

#
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
import sys
import subprocess


regulators = {
    'VCCINT': 'tps53681-i2c-0-60',
    'VCC_FPD': 'tps53681-i2c-0-60',
    'VCC_AIE': 'tps53681-i2c-0-61',
    'VCC_SOC': 'tps53681-i2c-0-61',
}

ina226s = {
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
    if (target == 'VCC_FPD' or target == 'VCC_SOC'):
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
    if (target == 'VCCINT' or target == 'VCC_AIE'):
        out = 'iout1.'
    elif (target == 'VCC_FPD' or target == 'VCC_SOC'):
        out = 'iout2:'
    else:
        out = 'iout1:'

    if line.find(out) > -1:
        tokens = line.split()
        if tokens[2] == 'mA':
            current = float(tokens[1]) / 1000.0
        else:
            current = float(tokens[1])

        if (target == 'VCCINT' or target == 'VCC_AIE'):
            total_current += current

    # Power
    if (target == 'VCCINT' or target == 'VCC_AIE'):
        out = 'pout1:'
    elif (target == 'VCC_FPD' or target == 'VCC_SOC'):
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
    if (target == 'VCCINT' or target == 'VCC_AIE'):
        print('Current(A):\t%.4f' % total_current)
    else:
        print('Current(A):\t%.4f' % current)

if (target in ina226s or target == 'VCCINT' or target == 'VCC_FPD' or \
    target == 'VCC_AIE' or target == 'VCC_SOC'):
    print('Power(W):\t%.4f' % wattage)

