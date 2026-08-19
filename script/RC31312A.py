#! /usr/bin/env python3
#
# Copyright (c) 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Renesas VersaClock 8 / RC31312A — programmed over I2C from RICBox .rbs settings files.
#
# Invoked like other vendor clocks (Vendor_Utility_Clock in sc_common.c):
#   RC31312A.py <Board> <I2C_Bus> <I2C_Addr> <Default_Design> <Command> <Clock_Name> [<Clock_Design>]
#
# Clock files (RICBox settings):
#   BIT/clock_files/RC31312A/<design>.rbs
#   /data/clock_files/<design>.rbs
#
# An .rbs file may contain multiple CONFIG sections (e.g. CONFIG:0 "No SSC", CONFIG:1 "-0.5% SSC").
# Select a section with  <file_basename>::<config name>  or  <file_basename>#<config name>.
# If omitted, CONFIG:0 is programmed.
#
# Register dumps use 16-bit byte addresses 0x000–0xA0F (Versa8 programming guide / RICBox export).
# Writes use I2C 2-byte offset mode: [off_msb, off_lsb, data] per export_writeall_offset_size=2.
#
# I2C access uses the board JSON I2C_Bus / I2C_Address only. When the clock sits behind an I2C
# mux in device tree, I2C_Bus must be the downstream adapter node (e.g. /dev/i2c-N for the mux
# channel); the kernel selects the mux — this script does not talk to the mux directly.
#

import errno
import os
import re
import sys

from smbus2 import SMBus, i2c_msg

Clock_Dir = "/usr/share/system-controller-app/.sc_app/vendor_clock/"
Default_CF_Dir = "/usr/share/system-controller-app/BIT/clock_files/"
Custom_CF_Dir = "/data/clock_files/"

Runtime_Ext = ".rbs"

# Offset#  BinaryValue  HexValue  Offset#
_REG_LINE = re.compile(
    r"^([0-9A-Fa-f]+)\s+[01]{8}\s+([0-9A-Fa-f]{1,2})\s+[0-9A-Fa-f]+\s*$"
)


def Parse_Clock_Design_Arg(Arg):
    """Return (file_basename, optional CONFIG selector)."""
    Line = Arg.split("\n")[0].strip()
    for Sep in ("::", "#"):
        if Sep in Line:
            Base, Sel = Line.split(Sep, 1)
            return Base.strip(), Sel.strip()
    return Line, None


def VC8_Write_Byte_2B(Bus, Dev, Reg16, Value):
    """I2C 2-byte offset write (Versa8 programming guide §2.2.2)."""
    Msg = i2c_msg.write(
        Dev,
        [(Reg16 >> 8) & 0xFF, Reg16 & 0xFF, Value & 0xFF],
    )
    Bus.i2c_rdwr(Msg)


def Discover_Clock(Clock_Name, Default_Design):
    """Resolve active design after setclock/setbootclock/restoreclock (vendor_clock stamp)."""
    Path = Clock_Dir + Clock_Name
    if os.path.exists(Path):
        try:
            with open(Path, encoding="utf-8", errors="replace") as F:
                print(F.readline().rstrip("\n\r"))
                return
        except OSError as E:
            print("ERROR: failed to open '%s': %s" % (Path, E))
            sys.exit(-1)
    print(Default_Design)


def Find_Clock_File(Chip, Design_Base, Extension):
    Name = Design_Base + Extension
    P = os.path.join(Default_CF_Dir, Chip, Name)
    if os.path.isfile(P):
        return P
    P = os.path.join(Custom_CF_Dir, Name)
    if os.path.isfile(P):
        return P
    return None


def Parse_RBS_File(Path, Config_Select=None):
    """
    Parse a RICBox .rbs file and return [(reg16, value), ...] for the chosen CONFIG.

    Config_Select may be CONFIG index (0, 1, …), CONFIG id string, or CONFIG Name field.
    None selects CONFIG:0.
    """
    Configs = []
    Current = None

    try:
        with open(Path, "r", encoding="utf-8", errors="replace") as F:
            for Raw in F:
                Line = Raw.strip()
                if not Line:
                    continue
                if Line.startswith("CONFIG:"):
                    if Current is not None:
                        Configs.append(Current)
                    try:
                        Idx = int(Line.split(":", 1)[1].strip())
                    except ValueError:
                        print("ERROR: invalid CONFIG line in '%s': %s" % (Path, Line))
                        sys.exit(-1)
                    Current = {"index": Idx, "id": None, "name": None, "regs": []}
                    continue
                if Line.startswith("========"):
                    continue
                if Current is None:
                    continue
                if Line.startswith("Data Fields"):
                    break
                if not Current["regs"]:
                    if Line.startswith("ID:"):
                        Current["id"] = Line.split(":", 1)[1].strip()
                        continue
                    if Line.startswith("Name:"):
                        Current["name"] = Line.split(":", 1)[1].strip()
                        continue
                    if Line.startswith("Offset#") and "BinaryValue" in Line:
                        continue
                Match = _REG_LINE.match(Line)
                if Match:
                    Reg = int(Match.group(1), 16)
                    Val = int(Match.group(2), 16)
                    Current["regs"].append((Reg, Val))
    except OSError as E:
        print("ERROR: failed to read RBS '%s': %s" % (Path, E))
        sys.exit(-1)

    if Current is not None:
        Configs.append(Current)

    if not Configs:
        print("ERROR: no CONFIG section in RBS '%s'" % Path)
        sys.exit(-1)

    if Config_Select is None:
        return Configs[0]["regs"]

    Sel = Config_Select.strip()
    M = re.fullmatch(r"(?i)(?:config:?)?(\d+)", Sel)
    if M:
        Want = int(M.group(1))
        for C in Configs:
            if C["index"] == Want:
                return C["regs"]
    else:
        Sel_L = Sel.lower()
        for C in Configs:
            if C["name"] and C["name"].lower() == Sel_L:
                return C["regs"]
            if C["id"] and C["id"].lower() == Sel_L:
                return C["regs"]

    Names = []
    for C in Configs:
        Label = C["name"] if C["name"] else ("CONFIG:%d" % C["index"])
        Names.append(Label)
    print(
        "ERROR: CONFIG '%s' not found in '%s' (available: %s)"
        % (Sel, Path, ", ".join(Names))
    )
    sys.exit(1)


def Format_I2C_Target(Bus_Path, Bus_Num, Addr):
    return "I2C bus %s (adapter %d), 7-bit address 0x%02x" % (
        Bus_Path,
        Bus_Num,
        Addr,
    )


def Report_Program_Error(Bus_Path, Bus_Num, Addr, RBS_Path, Err, Reg16=None):
    Target = Format_I2C_Target(Bus_Path, Bus_Num, Addr)
    if Reg16 is not None:
        Where = "register write at offset 0x%03x on %s" % (Reg16, Target)
    else:
        Where = "access to %s" % Target

    print("ERROR: programming '%s' failed during %s: %s" % (RBS_Path, Where, Err))

    if getattr(Err, "errno", None) == errno.ENXIO:
        print(
            "ERROR: %s is not reachable (no I2C ACK). Verify the board JSON "
            "I2C_Bus points at the device-tree adapter for this clock (use the "
            "mux downstream /dev/i2c-N if the part is behind an I2C mux), that "
            "adapter %s exists, and that address 0x%02x appears under "
            "/sys/bus/i2c/devices/ on that bus."
            % (Target, Bus_Path, Addr)
        )
    elif not os.path.exists(Bus_Path):
        print("ERROR: %s does not exist on this system" % Bus_Path)


def Program_Clock(Bus_Path, Bus_Num, I2C_Addr, Clock_Name, RBS_Path, Config_Select, Stamp_Design):
    Regs = Parse_RBS_File(RBS_Path, Config_Select)

    if not os.path.exists(Bus_Path):
        print("ERROR: %s does not exist; cannot program RC31312A at 0x%02x" % (
            Bus_Path,
            I2C_Addr,
        ))
        sys.exit(-1)

    try:
        with SMBus(Bus_Num, force=True) as Bus:
            for Reg16, Value in Regs:
                try:
                    VC8_Write_Byte_2B(Bus, I2C_Addr, Reg16, Value)
                except OSError as E:
                    Report_Program_Error(
                        Bus_Path, Bus_Num, I2C_Addr, RBS_Path, E, Reg16
                    )
                    sys.exit(-1)
    except OSError as E:
        Report_Program_Error(Bus_Path, Bus_Num, I2C_Addr, RBS_Path, E)
        sys.exit(-1)

    os.makedirs(Clock_Dir, exist_ok=True)
    Stamp = Clock_Dir + Clock_Name
    try:
        with open(Stamp, "w", encoding="utf-8") as F:
            F.write(Stamp_Design + "\n")
    except OSError as E:
        print("ERROR: failed to write '%s': %s" % (Stamp, E))
        sys.exit(-1)


def Remove_Boot_Clock_File(Clock_Name):
    Path = Clock_Dir + Clock_Name + "_boot"
    try:
        if os.path.isfile(Path):
            os.remove(Path)
            if hasattr(os, "sync"):
                os.sync()
    except OSError as E:
        print("ERROR: failed to remove '%s': %s" % (Path, E))
        sys.exit(-1)


def Create_Boot_Clock_File(Clock_Name, Clock_Design):
    """Persist boot selection when board has no auxiliary EEPROM programming path."""
    os.makedirs(Clock_Dir, exist_ok=True)
    Path = Clock_Dir + Clock_Name + "_boot"
    Content = Clock_Design + "\n"
    try:
        with open(Path, "w", encoding="utf-8") as F:
            F.write(Content)
        if hasattr(os, "sync"):
            os.sync()
    except OSError as E:
        print("ERROR: failed to write '%s': %s" % (Path, E))
        sys.exit(-1)


def Resolve_And_Program(Bus_Path, Bus_Num, I2C_Addr, Clock_Name, Clock_Design_Arg):
    File_Base, Config_Sel = Parse_Clock_Design_Arg(Clock_Design_Arg)
    RBS = Find_Clock_File(Chip, File_Base, Runtime_Ext)
    if not RBS:
        print("ERROR: failed to find design clock file '%s'" % File_Base)
        sys.exit(-1)
    Program_Clock(
        Bus_Path,
        Bus_Num,
        I2C_Addr,
        Clock_Name,
        RBS,
        Config_Sel,
        Clock_Design_Arg.strip(),
    )


#
# argv: Board I2C_Bus I2C_Addr Default_Design Command Clock_Name [Clock_Design ...]
#
Num_Args = len(sys.argv) - 1
if Num_Args < 6:
    print("ERROR: missing the required number of arguments")
    sys.exit(-1)

Chip = os.path.basename(sys.argv[0].strip(".py"))
I2C_Bus_Path = sys.argv[2]
I2C_Bus = int(I2C_Bus_Path.replace("/dev/i2c-", ""))
I2C_Addr = int(sys.argv[3], 0)

Default_Design = sys.argv[4]
Command = sys.argv[5]
Clock_Name = sys.argv[6]

if Command in ("setclock", "setbootclock"):
    if Num_Args < 7:
        print("ERROR: invalid number of arguments")
        sys.exit(-1)
    Clock_Design = " ".join(sys.argv[7:]).split("\n")[0].strip()

if Command == "getclock":
    Discover_Clock(Clock_Name, Default_Design)

elif Command == "setclock":
    Resolve_And_Program(I2C_Bus_Path, I2C_Bus, I2C_Addr, Clock_Name, Clock_Design)

elif Command == "setbootclock":
    Resolve_And_Program(I2C_Bus_Path, I2C_Bus, I2C_Addr, Clock_Name, Clock_Design)
    Create_Boot_Clock_File(Clock_Name, Clock_Design)

elif Command == "restoreclock":
    Resolve_And_Program(I2C_Bus_Path, I2C_Bus, I2C_Addr, Clock_Name, Default_Design)
    Remove_Boot_Clock_File(Clock_Name)

else:
    print("ERROR: invalid clock command")
    sys.exit(-1)
