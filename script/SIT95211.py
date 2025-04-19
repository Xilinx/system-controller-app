#! /usr/bin/env python3

#
# Copyright (c) 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import os
import sys
import time
import glob
import subprocess
from smbus2 import SMBus, i2c_msg

Runtime_Ext = ".csv"
EEPROM_Ext = ".eeprom.py"

Clock_Dir = "/usr/share/system-controller-app/.sc_app/vendor_clock/"
Default_CF_Dir = "/usr/share/system-controller-app/BIT/clock_files/"
Custom_CF_Dir = "/data/clock_files/"

global EEPROM_Size, EEPROM_Device_ID, EEPROM_Config_Word, EEPROM_Page_ID

#
# This function reads in a file with 'Runtime_Ext' format into a list
# of integer data.  The file format is two hex data per line represented
# in ASCII.
#
def Convert_EEPROM(File):
    Data = []
    try:
        with open(File, 'r') as f:
            for Line in f:
                Hex_Str = Line.strip()  # Remove leading/trailing whitespace and newlines
                Hex_Str = Hex_Str[2:]
                try:
                    Byte_Data = bytes.fromhex(Hex_Str)
                    Byte_List = list(Byte_Data)
                    for Byte in Byte_List:
                        Data.append(Byte)
                except:
                    print("ERROR: invalid hex value on line " + line.strip() + " of '" + File + "'")
                    exit(-1)
    except:
        print("ERROR: failed to open clock file '" + File + "'")
        exit(-1)
    return Data


#
# This function returns all the files with 'EEPROM_Ext' extension
# that are found in 'Directory' argument.
#
def EEPROM_Files(Directory):
    Files = []
    if os.path.exists(Directory):
        for File in os.listdir(Directory):
            if File.endswith(EEPROM_Ext):
                File_Path = os.path.join(Directory, File)
                Files.append(File_Path)
    return Files


#
# This function return True if the content of argument 'File' matches
# the content of EEPROM.
#
def EEPROM_Compare(File, I2C_Bus, I2C_Addr, Mode):
    if Mode == 0:
        EEPROM_Data = Convert_EEPROM(File)
    elif Mode == 1:
        # Assume EEPROM is blank if its first 256 bytes are 0xFF
        EEPROM_Data = [0xFF] * 256
    else:
        print("ERROR: invalid mode of EEPROM comparison")
        exit(-1)

    try:
        with SMBus(I2C_Bus, force=True) as Bus:
            Size = 32
            for I in range(0, (len(EEPROM_Data) - Size), Size):
                Offset = [((I >> 8) & 0xFF), (I & 0xFF)]
                Write_Msg = i2c_msg.write(I2C_Addr, Offset)
                Read_Msg = i2c_msg.read(I2C_Addr, Size)
                Bus.i2c_rdwr(Write_Msg, Read_Msg)
                Data = list(Read_Msg)
                for J in range(0, Size):
                    if Data[J] != EEPROM_Data[I + J]:
                        return False
            return True
    except:
        print("ERROR: failed to compare EEPROM with design '" + File + "'")
        exit(-1)


#
# This function programs EEPROM with a new clock design.
#
def Program_EEPROM(I2C_Bus, I2C_Addr, Clock_Design):
    EEPROM_Data = Convert_EEPROM(Clock_Design)
    try:
        with SMBus(I2C_Bus, force=True) as Bus:
            Size = 32
            for I in range(0, len(EEPROM_Data), Size):
                Addr_Data = [((I >> 8) & 0xFF), (I & 0xFF)] + EEPROM_Data[I:(I + Size)]
                Msg = i2c_msg.write(I2C_Addr, Addr_Data)
                Bus.i2c_rdwr(Msg)
                time.sleep(0.01)
    except:
        print("ERROR: failed to program EEPROM with design '" + Clock_Design + "'")
        exit(-1)


#
# This function programs a new runtime design on a clock.
#
def Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, New_Design):
    try:
        with open(New_Design, 'r') as f:
            Bus = SMBus(I2C_Bus, force=True)
            for Line in f:
                if Line.startswith('sleep'):
                    #time.sleep(0.01)
                    continue
                Offset, Data = Line.split(',')
                Addr_Data = [int(Offset, 16), int(Data, 16)]
                Msg = i2c_msg.write(I2C_Addr, Addr_Data)
                Bus.i2c_rdwr(Msg)
    except:
        print("ERROR: failed to open file '" + New_Design + "'")
        exit(-1)

    #
    # Record the new programmed design in 'Clock_Dir + Clock_Name' file
    # so function 'Discover_Clock' could reference it.
    #
    os.makedirs(Clock_Dir, exist_ok=True)
    try:
        with open(Clock_Dir + Clock_Name, 'w') as f:
            f.write(os.path.basename(New_Design.strip(Runtime_Ext)))
    except:
        print("ERROR: failed to create file '" + (Clock_Dir + Clock_Name) + "'")
        exit(-1)


#
# This function identifies which clock design is in-effect.
#
def Discover_Clock(I2C_Bus, I2C_Addr, Chip, Clock_Name, Default_Design):
    #
    # If the clock has already been custom-programmed, there will be a file
    # named after that clock under 'Clock_Dir' directory containing the design
    # name.
    #
    if os.path.exists(Clock_Dir + Clock_Name):
        try:
            with open(Clock_Dir + Clock_Name) as f:
                print(f.readline())
                return
        except:
            print("ERROR: failed to open '" + (Clock_Dir + Clock_Name) + "'")
            exit(-1)

    #
    # In case EEPROM has not previously been programmed, its content is all 0xFF.
    #
    if EEPROM_Compare("", I2C_Bus, I2C_Addr, 1):
        print(Default_Design)
        return

    #
    # In case there has not been previous custom design programming, find all
    # possible clock designs that are available for the clock.  Compare each
    # with current content of EEPROM to find out which design has been in-effect
    # since boot time.
    #
    Files = EEPROM_Files(Default_CF_Dir + Chip)
    Files += EEPROM_Files(Custom_CF_Dir)
    for File in Files:
        if EEPROM_Compare(File, I2C_Bus, I2C_Addr, 0):
            print(os.path.basename(File.strip(EEPROM_Ext)))
            return
    print("ERROR: could not identify the design for clock '" + Clock_Name + "'")
    exit(-1)


#
# Find a clock file
#
def Find_Clock_File(Chip, Design, Extension):
    File = Default_CF_Dir + Chip + "/" + Design + Extension
    if not (os.path.exists(File)):
        File = Custom_CF_Dir + Design + Extension
        if not (os.path.exists(File)):
            return None

    #
    # Validate that the design file for programming EEPROM is generated correctly
    #
    if (Extension == EEPROM_Ext):
        Command = ['/bin/head', '-1', File]
        Result = subprocess.run(Command, capture_output=True, text=True)
        EEPROM_Identifier = Result.stdout.strip().splitlines()[0]

        #
        # Strip the leading '0x' and convert the remaining into individual 2-character strings
        #
        First_Line = EEPROM_Identifier[2:]
        First_Four_Bytes = [First_Line[i:i+2] for i in range(0, len(First_Line), 2)]
        EEPROM_Identifier = [EEPROM_Size, EEPROM_Device_ID, EEPROM_Config_Word, EEPROM_Page_ID]
        if (First_Four_Bytes != EEPROM_Identifier):
            print("ERROR: file '" + File + "' is not generated correctly")
            exit(-1)

    return File


#
# Identify board revision
#
def Get_Board_Revision(FRU_Path):
    Arg = "--fru-file=" + FRU_Path
    Command = ['/usr/sbin/ipmi-fru', Arg]
    Result = subprocess.run(Command, capture_output=True, text=True)
    OnBoard_EEPROM = Result.stdout.strip().splitlines()
    Label = "  FRU Board Custom Info: "
    Revisions = ['A01']
    for Rev in Revisions:
        if Label + Rev in OnBoard_EEPROM:
            return Rev

    return "UNKNOWN"


#
# Main Function
#
Num_Args = len(sys.argv) - 1
if Num_Args < 6:
    print("ERROR: missing the required number of arguments")
    exit(-1)

Chip = os.path.basename(sys.argv[0].strip(".py"))
Board = sys.argv[1]
I2C_Bus = int(sys.argv[2].replace("/dev/i2c-", ""))
I2C_Addr = int(sys.argv[3])
Default_Design = sys.argv[4]
Command = sys.argv[5]
Clock_Name = sys.argv[6]

if (Board == "VEK385"):
    SiTime_EEPROM_Search = "/sys/bus/i2c/devices/*/eeprom_sit95*/nvmem"
    Bus_Address = glob.glob(SiTime_EEPROM_Search, recursive=True)[0].split('/')[5]
    EEPROM_I2C_Addr = int(Bus_Address[Bus_Address.find('-') + 1:], 16)

    #
    # Different revisions of the board have different default clock design
    #
    OnBoard_EEPROM_Search = "/sys/bus/i2c/devices/*/eeprom_cc*/nvmem"
    Board_Revision = "-" + Get_Board_Revision(glob.glob(OnBoard_EEPROM_Search, recursive=True)[0])
    Default_Design_Search = Default_CF_Dir + Chip + "/" + Board + Board_Revision + "*"
    if (len(glob.glob(Default_Design_Search)) != 0):
        Index = Default_Design.find(Board)
        Default_Design = Default_Design[:Index + len(Board)] + Board_Revision + \
                         Default_Design[Index + len(Board):]

    #
    # EEPROM identifier data
    #
    EEPROM_Size= "00"
    EEPROM_Device_ID = "69"
    EEPROM_Config_Word = "01"
    EEPROM_Page_ID = "00"


if (Command == "setclock" or Command == "setbootclock"):
    if not Num_Args == 7:
        print("ERROR: invalid number of arguments")
        exit(-1)
    else:
        Clock_Design = sys.argv[7]

if Command == "getclock":
    Discover_Clock(I2C_Bus, EEPROM_I2C_Addr, Chip, Clock_Name, Default_Design)

elif Command == "setclock":
    Runtime_File = Find_Clock_File(Chip, Clock_Design, Runtime_Ext)
    if not Runtime_File:
        print("ERROR: failed to find design clock file '" + Clock_Design + "'")
        exit(-1)

    Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File)

elif Command == "setbootclock":
    Runtime_File = Find_Clock_File(Chip, Clock_Design, Runtime_Ext)
    if not Runtime_File:
        print("ERROR: failed to find design clock file '" + Clock_Design + "'")
        exit(-1)

    EEPROM_File = Find_Clock_File(Chip, Clock_Design, EEPROM_Ext)
    if not EEPROM_File:
        print("ERROR: failed to find EEPROM clock file '" + Clock_Design + "'")
        exit(-1)

    Program_EEPROM(I2C_Bus, EEPROM_I2C_Addr, EEPROM_File)
    Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File)

elif Command == "restoreclock":
    Runtime_File = Find_Clock_File(Chip, Default_Design, Runtime_Ext)
    if not Runtime_File:
        print("ERROR: failed to find design clock file '" + Default_Design + "'")
        exit(-1)

    EEPROM_File = Find_Clock_File(Chip, Default_Design, EEPROM_Ext)
    if not EEPROM_File:
        print("ERROR: failed to find EEPROM clock file '" + Default_Design + "'")
        exit(-1)

    Program_EEPROM(I2C_Bus, EEPROM_I2C_Addr, EEPROM_File)
    Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File)

else:
    print("ERROR: invalid clock command")
    exit(-1)
