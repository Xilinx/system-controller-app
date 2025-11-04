#! /usr/bin/env python3

#
# Copyright (c) 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

import subprocess
import argparse

SI5518_Script = "/usr/share/system-controller-app/script/SI5518.py"

Chip = "SI5518"
Board = "VRK160"
I2C_Bus = 14
I2C_Addr = 96
Default_Design = "VRK160_SI5518"
Clock_Name = "Si5518_CLK"


# Argument parser
parser = argparse.ArgumentParser(
    description='Command Si5518 clock chip over i2c. NOTE: Custom files must reside in /data/clock_files/')

parser.add_argument('-r', '--restoreclock',
                    action='store_true',
                    help='Restore clock to default configuration.')

parser.add_argument('-g', '--getclock',
                    action='store_true',
                    help='Retrieve current configuration ID loaded in clock.')

parser.add_argument('-s', '--setclock',
                    type=str,
                    default=None,
                    help='Program clock with custom configuration. SETCLOCK argument is base name of config files.')


def main(args):
    Command = ["sc_app -c board"]
    Result = subprocess.run(Command, capture_output=True, text=True, shell=True)
    #print(Result.stdout)
    if "VRK160\n" != Result.stdout:
        print("ERROR: script only supports VRK160")
        exit(-1)

    Command = ["python3", SI5518_Script, Board, str(I2C_Bus), str(I2C_Addr), Default_Design]
    if args.getclock:
        Command.append("getclock")
        Command.append(Clock_Name)
        Result = subprocess.run(Command, capture_output=True, text=True)
        if Result.returncode != 0:
            print(f"ERROR: Command failed with return code {Result.returncode}")
            print(f"Standard Error:\n{Result.stderr}")
            exit(Result.returncode)
        else:
            print(Result.stdout.rstrip())

    elif args.setclock:
        Clock_Design = args.setclock
        Command.append("setclock")
        Command.append(Clock_Name)
        Command.append(Clock_Design)
        Result = subprocess.run(Command, capture_output=True, text=True)
        if Result.returncode != 0:
            print(f"ERROR: Command failed with return code {Result.returncode}")
            print(Result.stdout.rstrip())
            exit(Result.returncode)

    elif args.restoreclock:
        Command.append("restoreclock")
        Command.append(Clock_Name)
        Result = subprocess.run(Command, capture_output=True, text=True)

    else:
        print("ERROR: invalid clock command")
        exit(-1)


#
# Main Function
#
if __name__ == "__main__":
    try:
        main(parser.parse_args())

    except KeyboardInterrupt:
        print("Keyboard interrupt")
        exit(0)
