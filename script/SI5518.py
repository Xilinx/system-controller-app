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

Runtime_Ext = ".boot.hex.txt"
Config_Ext = ["-prod_fw", "-user_config", "-patch_rom"]

Clock_Dir = "/usr/share/system-controller-app/.sc_app/vendor_clock/"
Default_CF_Dir = "/usr/share/system-controller-app/BIT/clock_files/"
Custom_CF_Dir = "/data/clock_files/"

#
# SI5518 Device API Commands. See Si5518 Command/Property API Documentation by opening ClockBuilder Pro -> Documentation -> Device API
#
READ_REPLY = "00"
SIO_INFO = "02"
HOST_LOAD = "05"
BOOT = "07"
DEVICE_INFO = "08"
APP_INFO = "10"
RESTART = "F0"

#
# Device Register: 0x0FF0
#
LOWER_REG_BYTE = "F0"
UPPER_REG_BYTE = "0F"

global CMD_BUFFER_SIZE, REPLY_BUFFER_SIZE


#
# Checks that Si5518 device is ready for commands.
#
def Poll_Device_CTS(Bus, I2C_Addr):
    """
    Polls the device for 1 byte until the MSB of the byte is 1.
    Times out after 50 attempts with 10ms intervals between each attempt.

    Arguments:
    - Bus: SMBus object for I2C communication.
    - I2C_Addr: Address of the I2C device.
    """
    Attempts = 50
    for Attempt in range(Attempts):  # Maximum of 50 attempts
        # Wait briefly before polling again
        time.sleep(0.01)

        # Create a read message to poll for 1 byte
        Read_msg = i2c_msg.read(I2C_Addr, 1)
        Bus.i2c_rdwr(Read_msg)

        # Extract the response byte
        Response = list(Read_msg)[0]
        #print(str(attempt) + " - Reply: " + str(response))

        # Check if the most significant bit (MSB) is 1
        if Response & 0x80 == 0x80:  # MSB is 1
            #print(f"File {file_number}: Write acknowledged after {attempt + 1} attempts.")
            return

    else:
        print("ERROR: clock not ready within " + str(Attempts) + " polling attempts")


#
# This function retrieves the command and reply buffer size.
#
def Obtain_Buffer_Size(I2C_Bus, I2C_Addr):
    try:
        with SMBus(I2C_Bus) as Bus:

            # Send SIO_INFO command to obtain the buffer size the device can receive and at most how much will transmit.
            Addr_Data = [int(LOWER_REG_BYTE, 16), int(UPPER_REG_BYTE, 16), int(SIO_INFO, 16)]
            Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data)
            Bus.i2c_rdwr(Write_Msg)

            Poll_Device_CTS(Bus, I2C_Addr)

            Read_Msg = i2c_msg.read(I2C_Addr, 5)
            Bus.i2c_rdwr(Read_Msg)
            Msg = list(Read_Msg)

            Cmd_Buffer_Size = ((Msg[2] << 8) | Msg[1])
            Reply_Buffer_Size = ((Msg[4] << 8) | Msg[3])

            return Cmd_Buffer_Size, Reply_Buffer_Size

    except:
        print("ERROR: failed to get buffer sizes")
        exit(-1)


#
# Retrieve configuration ID loaded into clock.
#
def Clock_ID(I2C_Bus, I2C_Addr):
    try:
        with SMBus(I2C_Bus) as Bus:

            # Send APP_INFO command to obtain the loaded configuration ID.
            Addr_Data = [int(LOWER_REG_BYTE, 16), int(UPPER_REG_BYTE, 16), int(APP_INFO, 16)]
            Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data)
            Bus.i2c_rdwr(Write_Msg)

            Poll_Device_CTS(Bus, I2C_Addr)

            Read_Msg = i2c_msg.read(I2C_Addr, 25)
            Bus.i2c_rdwr(Read_Msg)
            # Configuration ID has limit of 8 bytes.
            Device_ID = ''.join([chr(num) for num in list(Read_Msg)[11:19]])

            if Board in Device_ID:
                print(Default_Design)
            else:
                print(Device_ID + "\n")

    except:
        print("ERROR: failed to obtain clock config ID")
        exit(-1)


#
# Clears configuration on device. If boot command is given without loading custom config, default settings will be applied from NVM.
# Mode - 0: Wait for HOST_LOAD and BOOT command (custom configuration)
#        1: Automatically boot NVM contents (default image)
#
def Restart_Dev(I2C_Bus, I2C_Addr, Mode):
    if Mode != 0 and Mode != 1:
        print("ERROR: invalid mode provided for Si5518 restart command.")
        exit(-1)

    try:
        with SMBus(I2C_Bus) as Bus:

            # Send RESTART command to clear configuration in preparation for new image.
            Addr_Data = [int(LOWER_REG_BYTE, 16), int(UPPER_REG_BYTE, 16), int(RESTART, 16), Mode]
            Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data)
            Bus.i2c_rdwr(Write_Msg)

            Poll_Device_CTS(Bus, I2C_Addr)

    except Exception as e:
        print(f"An exception occurred: {e}")
        print(f"Type of exception: {type(e)}")
        print(f"Name of exception: {type(e).__name__}")
        print("ERROR: failed to restart clock")
        exit(-1)


#
# Starts clock with loaded configuration.
#
def Boot_Dev(I2C_Bus, I2C_Addr):
    try:
        with SMBus(I2C_Bus) as Bus:

            # Send BOOT command boot clock with loaded image.
            Addr_Data = [int(LOWER_REG_BYTE, 16), int(UPPER_REG_BYTE, 16), int(BOOT, 16)]
            Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data)
            Bus.i2c_rdwr(Write_Msg)

            Poll_Device_CTS(Bus, I2C_Addr)

    except:
        print("ERROR: failed to boot clock")
        exit(-1)


#
# This function programs a new runtime design on a clock.
#
def Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, New_Designs):
    Data_Index = 0
    Data_Arr = []
    Addr_Data = [int(LOWER_REG_BYTE, 16), int(UPPER_REG_BYTE, 16), int(HOST_LOAD, 16)]

    if len(New_Designs) < 2:
        raise ValueError("New_Design must contain at least 2 file paths.")

    # RESTART device and put in WAIT state for reconfiguring with other image.
    Restart_Dev(I2C_Bus, I2C_Addr, 0)

    Bus = SMBus(I2C_Bus, force=True)
    for Design in New_Designs:
        try:
            with open(Design, 'r') as f:
                for Line in f:
                    Data_Arr.append(int(Line, 16))
                    Data_Index += 1

                    # Device can only receive CMD_BUFFER_SIZE bytes at a time.
                    # Send configuration data once buffer limit is reached. Sending
                    # (CMD_BUFFER_SIZE - 4) bytes as a precaution to avoid pushing
                    # device into error state.
                    if Data_Index == (CMD_BUFFER_SIZE - 4):
                        Data_Index = 0
                        Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data + Data_Arr)
                        Bus.i2c_rdwr(Write_Msg)

                        Poll_Device_CTS(Bus, I2C_Addr)
                        Data_Arr.clear()

                else:
                    Write_Msg = i2c_msg.write(I2C_Addr, Addr_Data + Data_Arr)
                    Bus.i2c_rdwr(Write_Msg)

                    Poll_Device_CTS(Bus, I2C_Addr)

                #print(f"File {Design}: Finished processing.")

        except Exception as e:
            # Handle exceptions during the operation
            print(f"An exception of type {type(e).__name__} occurred: {e}")
            print("ERROR: failed to program configuration '" + Clock_Name + "'")
            exit(-1)

    # Boot device after flashing volatile memory with custom image.
    Boot_Dev(I2C_Bus, I2C_Addr)


#
# Find 2-3 clock files with BASE_NAME.
#    Files: BASE_NAME_fw.boot.hex.txt
#           BASE_NAME_config.boot.hex.txt
#           BASE_NAME_patch_rom.boot.hex.txt (optional: not all configurations produce this file)
#    Note: Files are produced by ClockBuilder Pro. Open Project -> Export -> Create Boot Files -> Hex file -> Save Boot Files
#
def Find_Clock_Files(Chip, Design, Extension):
    Files = []
    Missing_Files = []
    for Config in Config_Ext:
        File_Name = Design + Config + Extension
        File = Default_CF_Dir + Chip + "/" + File_Name
        #print("Looking for: " + File)
        if not (os.path.exists(File)):
            File = Custom_CF_Dir + File_Name
            if not (os.path.exists(File)):
                Missing_Files.append(File_Name)
                continue

        Files.append(File)

    # If either user_config or prof_fw are missing, exit without attempting to
    # reconfigure the clock.
    if any(Config_Ext[2] not in item for item in Missing_Files):
        for F in Missing_Files:
            if Config_Ext[2] not in F:
                print("ERROR: cannot find file " + F)
        exit(-1)

    return Files


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


if (Command == "setclock"):
    if not Num_Args == 7:
        print("ERROR: invalid number of arguments")
        exit(-1)
    else:
        Clock_Design = sys.argv[7]

CMD_BUFFER_SIZE, REPLY_BUFFER_SIZE = Obtain_Buffer_Size(I2C_Bus, I2C_Addr)

if Command == "getclock":
    Clock_ID(I2C_Bus, I2C_Addr)

elif Command == "setclock":
    Runtime_Files = Find_Clock_Files(Chip, Clock_Design, Runtime_Ext)
    if not Runtime_Files:
        print("ERROR: failed to find design clock file '" + Clock_Design + "'")
        exit(-1)

    Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_Files)

elif Command == "setbootclock":
    print("ERROR: command not supported for Si5518: " + Command)

elif Command == "restoreclock":
    # RESTART device and boot with image stored in the device's NVM.
    Restart_Dev(I2C_Bus, I2C_Addr, 1)

else:
    print("ERROR: invalid clock command")
    exit(-1)
