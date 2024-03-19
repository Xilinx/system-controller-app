#! /usr/bin/env python3

#
# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import sys
import argparse
import time
from smbus2 import SMBus, i2c_msg

def i2c_write(bus, dev, reg, val):
    data = [reg & 0xff]
    data += val
    msg = i2c_msg.write(dev, data)
    try:
        bus.i2c_rdwr(msg)
    except:
        print("Error accessing I2C device %02x" % dev)
        exit(-1)

def i2c_read(bus, dev, reg, size):
    data = [reg & 0xff]
    write = i2c_msg.write(dev, data)
    read = i2c_msg.read(dev, size)
    try:
        bus.i2c_rdwr(write, read)
    except:
        print("Error accessing I2C device %02x" % dev)
        exit(-1)
    return list(read)

def i2c_write_a16(bus, dev, reg, val):
    data = [reg >> 8, reg & 0xff]
    data += val
    msg = i2c_msg.write(dev, data)
    try:
        bus.i2c_rdwr(msg)
    except:
        print("Error accessing I2C device %02x" % dev)
        exit(-1)

def i2c_read_a16(bus, dev, reg, size):
    data = [reg >> 8, reg & 0xff]
    write = i2c_msg.write(dev, data)
    read = i2c_msg.read(dev, size)
    try:
        bus.i2c_rdwr(write, read)
    except:
        print("Error accessing I2C device %02x" % dev)
        exit(-1)
    return list(read)

EEPROM_PAGE=0xCF
EEPROM_I2C_ADDR=0x68+0
EEPROM_SIZE=0x68+1
EEPROM_OFFSET=0x68+2
EEPROM_CMD=0x68+4
EEPROM_BUF=0x80

EEPROM_STAT_PAGE=0xc0
EEPROM_STAT_REG=0x14+0x08

def wait_ready(bus, dev):
    i2c_write(bus, dev, 0xFC, [0x00, EEPROM_STAT_PAGE, 0x10, 0x20])
    cmd = [0, 0]
    MAX_RETRY=500
    retry=0
    while cmd == [0, 0] and retry < MAX_RETRY:
        cmd = i2c_read(bus, dev, EEPROM_STAT_REG, 2)
        retry += 1

    if retry == MAX_RETRY:
        return -1

    i2c_write(bus, dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])
    return 0

def erase_clock(busnum, dev):
    bus=SMBus(busnum)

    # Reset the page address.  supposed to do this once after reset
    i2c_write(bus, dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])

    eep_addr = 0

    data=[]
    for i in range(0,128):
        data.append(0xff)

    i2c_write(bus, dev, EEPROM_BUF, data)

    #Set eeprom address to 0x54
    i2c_write(bus, dev, EEPROM_I2C_ADDR, [0x50 + ((eep_addr & 0x10000)>>14)])
    #set write size
    i2c_write(bus, dev, EEPROM_SIZE, [128])
    #set eeprom offset
    i2c_write(bus, dev, EEPROM_OFFSET, [eep_addr & 0xff, (eep_addr >> 8) & 0xff])
    #set the write command
    i2c_write(bus, dev, EEPROM_CMD, [0x02, 0xEE])

    if wait_ready(bus, dev) != 0:
        print("1 Error accessing EEPROM")
        exit(-1)

    eep_addr = 0xF000

    #Set eeprom address to 0x54
    i2c_write(bus, dev, EEPROM_I2C_ADDR, [0x50 + ((eep_addr & 0x10000)>>14)])
    #set write size
    i2c_write(bus, dev, EEPROM_SIZE, [128])
    #set eeprom offset
    i2c_write(bus, dev, EEPROM_OFFSET, [eep_addr & 0xff, (eep_addr >> 8) & 0xff])
    #set the write command
    i2c_write(bus, dev, EEPROM_CMD, [0x02, 0xEE])

    if wait_ready(bus, dev) != 0:
        print("2 Error accessing EEPROM")
        exit(-1)

    return 0

def verify_clock(filename, busnum, dev):
    try:
        with open(filename, 'rb') as file:
            eep_dat = file.read()
    except:
        print("Failed to open file", filename)
        exit(-1)

    bus=SMBus(busnum)

    # Reset the page address.  supposed to do this once after reset
    i2c_write(bus, dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])

    eep_addr = 0
    while eep_addr < len(eep_dat):
        remain = len(eep_dat) - eep_addr
        if (remain < 128):
            size = remain
        else:
            size = 128

        data=[]
        for i in range(0,size):
            data.append(eep_dat[i + eep_addr])

        #Set eeprom address to 0x54
        i2c_write(bus, dev, EEPROM_I2C_ADDR, [0x50 + ((eep_addr & 0x10000)>>14)])
        #set write size
        i2c_write(bus, dev, EEPROM_SIZE, [size])
        #set eeprom offset
        i2c_write(bus, dev, EEPROM_OFFSET, [eep_addr & 0xff, (eep_addr >> 8) & 0xff])
        #set the write command
        i2c_write(bus, dev, EEPROM_CMD, [0x01, 0xEE])

        if wait_ready(bus, dev) != 0:
            print("Error accessing EEPROM")
            exit(-1)

        print("Addr %04x:%04x" % (eep_addr, len(eep_dat))) 

        check_data = i2c_read(bus, dev, EEPROM_BUF, size)
        for i in range(0, size):
            if data[i] != check_data[i]:
                print("Data mismatch at %05x, got %02x expected %02x" %(eep_addr+i, check_data[i], data[i]))
                exit(1)

        eep_addr += size
    print("Verification Complete")
    return 0

def program_clock(filename, busnum, dev):
    try:
        with open(filename, 'rb') as file:
            eep_dat = file.read()
    except:
        print("Failed to open file", filename)
        exit(-1)

    bus=SMBus(busnum)

    # Reset the page address.
    i2c_write(bus, dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])

    eep_addr = 0
    while eep_addr < len(eep_dat):
        remain = len(eep_dat) - eep_addr
        if (remain < 128):
            size = remain
        else:
            size = 128

        data=[]
        for i in range(0,size):
            data.append(eep_dat[i + eep_addr])

        i2c_write(bus, dev, EEPROM_BUF, data)

        #Set eeprom address to 0x54
        i2c_write(bus, dev, EEPROM_I2C_ADDR, [0x50 + ((eep_addr & 0x10000)>>14)])
        #set write size
        i2c_write(bus, dev, EEPROM_SIZE, [size])
        #set eeprom offset
        i2c_write(bus, dev, EEPROM_OFFSET, [eep_addr & 0xff, (eep_addr >> 8) & 0xff])
        #set the write command
        i2c_write(bus, dev, EEPROM_CMD, [0x02, 0xEE])

        if wait_ready(bus, dev) != 0:
            print("Error accessing EEPROM")
            exit(-1)
        print("Addr %04x:%04x" % (eep_addr, len(eep_dat)))
        eep_addr += size

    # Reset chip
    i2c_write(bus, dev, 0xFC, [0x00, 0xC0, 0x10, 0x20])
    i2c_write(bus, dev, 0x12, [0x5a])
    print("Programming Complete")
    return 0


def auto_int(x):
    return int(x, 0)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', dest='filename', help="Clock programming bin file")
    parser.add_argument('-b', dest='bus', type=auto_int, default=11, help="I2C Bus number")
    parser.add_argument('-d', dest='dev', type=auto_int, default=0x5b, help="I2C Device Address")
    parser.add_argument('-v', dest='verify', default=False, action=argparse.BooleanOptionalAction, help="verify")
    parser.add_argument('-e', dest='erase', default=False, action=argparse.BooleanOptionalAction, help="erase")
    args = parser.parse_args()

    if (args.verify):
        print(f"Filename %s, bus %d, dev %02x" % (args.filename, args.bus, args.dev))
        ret = verify_clock(args.filename, args.bus, args.dev)
    elif (args.erase):
        print(f"Erase bus %d, dev %02x" % (args.bus, args.dev))
        ret = erase_clock(args.bus, args.dev)
    else:
        print(f"Filename %s, bus %d, dev %02x" % (args.filename, args.bus, args.dev))
        ret = program_clock(args.filename, args.bus, args.dev)

    exit(ret)
