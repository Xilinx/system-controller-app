#!/usr/bin/env python3
#
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# 8A34001 clock Python script — clock files under BIT/clock_files/8A34001
# (.tcs, .txt, .bin).
#

import os
import sys

from smbus2 import SMBus, i2c_msg

# Vendor clock record and default/custom clock file directories (system-controller-app layout).
Clock_Dir = "/usr/share/system-controller-app/.sc_app/vendor_clock/"
Default_CF_Dir = "/usr/share/system-controller-app/BIT/clock_files/"
Custom_CF_Dir = "/data/clock_files/"

# Runtime image extensions: .tcs / .txt pair, optional .bin for 8A34001's EEPROM.
Runtime_Ext1 = ".tcs"
Runtime_Ext2 = ".txt"
EEPROM_Ext = ".bin"

# --- 8A34001's EEPROM access ---

EEPROM_PAGE = 0xCF
EEPROM_I2C_ADDR = 0x68 + 0
EEPROM_SIZE = 0x68 + 1
EEPROM_OFFSET = 0x68 + 2
EEPROM_CMD = 0x68 + 4
EEPROM_BUF = 0x80

#
# 8A34001's EEPROM is 128KiB split into two 64KiB banks. The byte written to EEPROM_I2C_ADDR is the
# I2C-style slave used for that access: base 0x50 for addresses 0x0000–0xFFFF, and 0x54 when bit 16
# of the linear EEPROM_Address is set (0x10000–0x1FFFF). (EEPROM_Address & 0x10000) >> 14 equals 0 or 4
# because 0x10000 >> 14 == 4. EEPROM_OFFSET still carries the 16-bit offset within the current bank.
#

EEPROM_STAT_PAGE = 0xC0
EEPROM_STAT_REG = 0x14 + 0x08

# 2-byte addressing preamble (legacy 8A34001 register load)
ADDR_MODE_2BYTE = bytes([0xFF, 0xFD, 0x00, 0x10, 0x20])


#
# I2C_Write — Write register Reg on I2C device Dev with payload Val (single smbus write transaction).
#
def I2C_Write(Bus, Dev, Reg, Val):
    Data = [Reg & 0xFF]
    Data += Val
    Msg = i2c_msg.write(Dev, Data)
    try:
        Bus.i2c_rdwr(Msg)
    except OSError:
        print("ERROR: failed to access I2C device %02x" % Dev)
        sys.exit(-1)


#
# I2C_Read — Read Size bytes from register Reg on I2C device Dev; returns a list of byte values.
#
def I2C_Read(Bus, Dev, Reg, Size):
    Data = [Reg & 0xFF]
    Write = i2c_msg.write(Dev, Data)
    Read = i2c_msg.read(Dev, Size)
    try:
        Bus.i2c_rdwr(Write, Read)
    except OSError:
        print("ERROR: failed to access I2C device %02x" % Dev)
        sys.exit(-1)
    return list(Read)


#
# Wait_Ready — Poll EEPROM status until ready or timeout; reselect EEPROM page. Returns 0 on success, -1 on timeout.
#
def Wait_Ready(Bus, Dev):
    I2C_Write(Bus, Dev, 0xFC, [0x00, EEPROM_STAT_PAGE, 0x10, 0x20])
    Cmd = [0, 0]
    Max_Retry = 500
    Retry = 0
    while Cmd == [0, 0] and Retry < Max_Retry:
        Cmd = I2C_Read(Bus, Dev, EEPROM_STAT_REG, 2)
        Retry += 1

    if Retry == Max_Retry:
        return -1

    I2C_Write(Bus, Dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])
    return 0


#
# I2C_Write_Raw — Send Payload to Dev as one I2C write (no leading register byte; used for runtime programming).
#
def I2C_Write_Raw(Bus, Dev, Payload):
    Msg = i2c_msg.write(Dev, list(Payload))
    try:
        Bus.i2c_rdwr(Msg)
    except OSError as E:
        print("ERROR: failed to access I2C device %02x: %s" % (Dev, E))
        sys.exit(-1)


#
# EEPROM_Files — List .bin paths for EEPROM identity: board-prefixed default bins plus every .bin in Custom_CF_Dir.
#
def EEPROM_Files(Board, Chip):
    Out = []
    DefaultDir = os.path.join(Default_CF_Dir, Chip)
    if os.path.isdir(DefaultDir):
        for Name in os.listdir(DefaultDir):
            if Name.endswith(EEPROM_Ext) and Name.startswith(Board):
                Out.append(os.path.join(DefaultDir, Name))
    if os.path.isdir(Custom_CF_Dir):
        for Name in os.listdir(Custom_CF_Dir):
            if Name.endswith(EEPROM_Ext):
                Out.append(os.path.join(Custom_CF_Dir, Name))
    return Out


# Unprogrammed EEPROM: first BLANK_EEPROM_COMPARE_LEN bytes read as 0xFF (reference when Filename is None).
BLANK_EEPROM_COMPARE_LEN = 256
BLANK_EEPROM_PATTERN = [0xFF] * BLANK_EEPROM_COMPARE_LEN


#
# EEPROM_Compare — True if 8A34001's EEPROM matches the reference. With Filename a path, read the file and
# compare the full length to EEPROM. With Filename None, compare only the first BLANK_EEPROM_COMPARE_LEN
# bytes to BLANK_EEPROM_PATTERN.
#
def EEPROM_Compare(Bus_Num, Dev, Filename):
    if Filename is None:
        EEPROM_Data = bytes(BLANK_EEPROM_PATTERN)
    else:
        try:
            with open(Filename, "rb") as F:
                EEPROM_Data = F.read()
        except OSError:
            return False

    Bus = SMBus(Bus_Num)

    try:
        I2C_Write(Bus, Dev, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])

        EEPROM_Address = 0
        while EEPROM_Address < len(EEPROM_Data):
            Remain = len(EEPROM_Data) - EEPROM_Address
            Size = Remain if Remain < 128 else 128

            Data = [EEPROM_Data[I + EEPROM_Address] for I in range(0, Size)]

            # Target I2C address for this 64K window (base 0x50 plus high-address bits).
            I2C_Write(Bus, Dev, EEPROM_I2C_ADDR, [0x50 + ((EEPROM_Address & 0x10000) >> 14)])
            # Byte count for this chunk (1..128).
            I2C_Write(Bus, Dev, EEPROM_SIZE, [Size])
            # 16-bit offset within the image (LSB, MSB).
            I2C_Write(Bus, Dev, EEPROM_OFFSET, [EEPROM_Address & 0xFF, (EEPROM_Address >> 8) & 0xFF])
            # Start read: command loads data from EEPROM into EEPROM_BUF for I2C_Read below.
            I2C_Write(Bus, Dev, EEPROM_CMD, [0x01, 0xEE])

            if Wait_Ready(Bus, Dev) != 0:
                return False

            Check_Data = I2C_Read(Bus, Dev, EEPROM_BUF, Size)
            for I in range(0, Size):
                if Data[I] != Check_Data[I]:
                    return False

            EEPROM_Address += Size
    except OSError:
        return False

    return True


#
# Preprocess_Size_Line — Normalize a Size: line from a .txt image: strip 0x prefixes and commas.
#
def Preprocess_Size_Line(Line):
    # Mutable copy for per-character edits.
    Chars = list(Line)
    Walk = 0
    while Walk < len(Chars):
        # Turn "0x" into spaces so hex fields split cleanly on ':'.
        if Chars[Walk] == "x" and Walk > 0:
            Chars[Walk - 1] = " "
            Chars[Walk] = " "
        if Chars[Walk] == ",":
            # Commas are not field separators; treat as whitespace.
            Chars[Walk] = " "
        Walk += 1
    return "".join(Chars)


#
# First_Hex_Int — First contiguous hex integer in Segment.
#
def First_Hex_Int(Segment):
    S = Segment
    I = 0
    while I < len(S) and S[I].isspace():
        I += 1
    Start = I
    while I < len(S):
        C = S[I]
        if "0" <= C <= "9" or "a" <= C <= "f" or "A" <= C <= "F":
            I += 1
            continue
        break
    if Start == I:
        raise ValueError
    return int(S[Start:I], 16)


#
# Parse_Hex_Nibbles — Parse contiguous hex digit pairs from Data_String into byte values.
#
def Parse_Hex_Nibbles(Data_String):
    Out = []
    Walk = 0
    S = Data_String
    while Walk < len(S):
        if S[Walk].isspace():
            Walk += 1
            continue
        if Walk + 1 >= len(S):
            # Lone trailing character; no full byte.
            break
        # One byte = two hex digits (high, low nibble).
        C1, C2 = S[Walk], S[Walk + 1]

        # Nibble — map one hex character to its 0..15 value.
        def Nibble(Ch):
            if "a" <= Ch <= "f":
                return ord(Ch) - 0x57
            if "A" <= Ch <= "F":
                return ord(Ch) - 0x37
            if "0" <= Ch <= "9":
                return ord(Ch) - 0x30
            raise ValueError("bad hex")

        # Pack MSB..LSB.
        Out.append((Nibble(C1) << 4) | Nibble(C2))
        # Consumed two input characters.
        Walk += 2
    return Out


#
# Program_EEPROM — Stream Clock_Design (.bin path) to 8A34001's EEPROM in 128-byte chunks;
# finalize with device-specific postamble.
#
def Program_EEPROM(I2C_Bus, I2C_Addr, Clock_Design):
    try:
        with open(Clock_Design, "rb") as F:
            EEPROM_Data = F.read()
    except OSError:
        print("ERROR: failed to open file %s" % Clock_Design)
        sys.exit(-1)

    Bus = SMBus(I2C_Bus)

    I2C_Write(Bus, I2C_Addr, 0xFC, [0x00, EEPROM_PAGE, 0x10, 0x20])

    EEPROM_Address = 0
    while EEPROM_Address < len(EEPROM_Data):
        Remain = len(EEPROM_Data) - EEPROM_Address
        Size = Remain if Remain < 128 else 128

        Data = [EEPROM_Data[I + EEPROM_Address] for I in range(0, Size)]

        # Stage this chunk in EEPROM_BUF before issuing the program command.
        I2C_Write(Bus, I2C_Addr, EEPROM_BUF, Data)

        # Target I2C address for this 64K window (base 0x50 plus high-address bits).
        I2C_Write(Bus, I2C_Addr, EEPROM_I2C_ADDR, [0x50 + ((EEPROM_Address & 0x10000) >> 14)])
        # Byte count for this chunk (1..128).
        I2C_Write(Bus, I2C_Addr, EEPROM_SIZE, [Size])
        # 16-bit offset within the image (LSB, MSB).
        I2C_Write(Bus, I2C_Addr, EEPROM_OFFSET, [EEPROM_Address & 0xFF, (EEPROM_Address >> 8) & 0xFF])
        # Start program: 0x02,0xEE writes EEPROM_BUF to EEPROM at OFFSET (Wait_Ready polls completion).
        I2C_Write(Bus, I2C_Addr, EEPROM_CMD, [0x02, 0xEE])

        if Wait_Ready(Bus, I2C_Addr) != 0:
            print("ERROR: failed to access EEPROM")
            sys.exit(-1)
        EEPROM_Address += Size

    I2C_Write(Bus, I2C_Addr, 0xFC, [0x00, 0xC0, 0x10, 0x20])
    I2C_Write(Bus, I2C_Addr, 0x12, [0x5A])
    return 0


#
# Program_Clock — Program runtime from New_Design (.txt) via I2C Size: blocks; record design name in
# Clock_Dir + Clock_Name for getclock (basename without Runtime_Ext2).
#
def Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, New_Design):
    Bus = SMBus(I2C_Bus, force=True)

    I2C_Write_Raw(Bus, I2C_Addr, ADDR_MODE_2BYTE)

    try:
        with open(New_Design, "r", encoding="utf-8", errors="replace") as FP:
            for Raw_Line in FP:
                if "Size:" not in Raw_Line:
                    continue
                Buffer = Preprocess_Size_Line(Raw_Line)
                Tokens = Buffer.split(":")
                if len(Tokens) < 4:
                    continue
                try:
                    Size = First_Hex_Int(Tokens[1])
                    Offset = First_Hex_Int(Tokens[2]) & 0xFF
                    Data_String = ":".join(Tokens[3:]).strip()
                except ValueError:
                    continue

                Payload_Bytes = Parse_Hex_Nibbles(Data_String)
                if len(Payload_Bytes) != Size:
                    print(
                        "ERROR: Size field %d but parsed %d data bytes in line"
                        % (Size, len(Payload_Bytes))
                    )
                    sys.exit(-1)

                Data = [Offset] + Payload_Bytes
                I2C_Write_Raw(Bus, I2C_Addr, Data)
    except OSError as E:
        print("ERROR: failed to open file %s: %s" % (New_Design, E))
        sys.exit(-1)

    # Record programmed design in Clock_Dir + Clock_Name for Discover_Clock.
    os.makedirs(Clock_Dir, exist_ok=True)
    try:
        with open(Clock_Dir + Clock_Name, "w", encoding="utf-8") as F:
            F.write(os.path.basename(New_Design.strip(Runtime_Ext2)) + "\n")
    except OSError as E:
        print("ERROR: failed to create file '%s': %s" % (Clock_Dir + Clock_Name, E))
        sys.exit(-1)


#
# Discover_Clock — Identify which clock design is in effect (getclock). Uses vendor_clock
# record, .bin EEPROM image matching, and a fixed-length blank-EEPROM check. Prints one line:
# design basename or Default_Design.
#
def Discover_Clock(I2C_Bus, I2C_Addr, Chip, Board, Clock_Name, Default_Design):
    #
    # If the clock was programmed via setclock / setbootclock / restoreclock, Program_Clock
    # records the active design under Clock_Dir + Clock_Name (first line is the design name).
    #
    if os.path.exists(Clock_Dir + Clock_Name):
        try:
            with open(Clock_Dir + Clock_Name, encoding="utf-8") as F:
                print(F.readline().rstrip("\n\r"))
            return
        except OSError as E:
            print("ERROR: failed to open '%s': %s" % (Clock_Dir + Clock_Name, E))
            sys.exit(-1)

    #
    # If 8A34001's EEPROM was not programmed, the leading region reads as 0xFF; report the board
    # default design (EEPROM_Compare with Filename None / BLANK_EEPROM_COMPARE_LEN).
    #
    if EEPROM_Compare(I2C_Bus, I2C_Addr, None):
        print(Default_Design)
        return

    #
    # Otherwise EEPROM holds a programmed image. Collect candidate .bin paths for this board
    # and chip (default then custom), compare each file to EEPROM's contents, and print the basename
    # of the first match (extension stripped). This infers which image has been in effect.
    #
    for Path in EEPROM_Files(Board, Chip):
        if EEPROM_Compare(I2C_Bus, I2C_Addr, Path):
            Base = os.path.basename(Path)
            if Base.endswith(EEPROM_Ext):
                print(Base[: -len(EEPROM_Ext)])
            else:
                print(Base)
            return

    #
    # No vendor_clock file and EEPROM did not match any known .bin; report Default_Design.
    #
    print(Default_Design)


#
# Clock_Design_Basename — Normalize a design string: first line, strip ends; empty after strip exits.
# For argv[7+] (setclock/setbootclock): if tokens are <Base>.tcs <Base>.txt [ <Base>.bin ] (same Base),
# return Base; else return the stripped line. Also used for argv[4] default design (restoreclock/getclock).
#
def Clock_Design_Basename(Arg):
    Line = Arg.split("\n")[0].strip()
    if not Line:
        print("ERROR: invalid clock_files string")
        sys.exit(-1)
    Parts = Line.split()
    if len(Parts) == 2:
        Base1, Ext1 = os.path.splitext(os.path.basename(Parts[0]))
        Base2, Ext2 = os.path.splitext(os.path.basename(Parts[1]))
        Ext1, Ext2 = Ext1.lower(), Ext2.lower()
        if Ext1 == Runtime_Ext1 and Ext2 == Runtime_Ext2 and Base1 == Base2:
            return Base1
    if len(Parts) == 3:
        Base1, Ext1 = os.path.splitext(os.path.basename(Parts[0]))
        Base2, Ext2 = os.path.splitext(os.path.basename(Parts[1]))
        Base3, Ext3 = os.path.splitext(os.path.basename(Parts[2]))
        Ext1, Ext2, Ext3 = Ext1.lower(), Ext2.lower(), Ext3.lower()
        if (
            Ext1 == Runtime_Ext1
            and Ext2 == Runtime_Ext2
            and Ext3 == EEPROM_Ext
            and Base1 == Base2 == Base3
        ):
            return Base1
    return Line


#
# Find_Clock_File — Design is a bare name from Clock_Design_Basename (no .tcs/.txt/.bin suffix).
# Name = Design + Extension. Probe Default_CF_Dir/<Chip>/ then Custom_CF_Dir. Returns None if missing.
# Exits on invalid Extension selector.
#
def Find_Clock_File(Chip, Design, Extension):
    if Extension not in (Runtime_Ext1, Runtime_Ext2, EEPROM_Ext):
        print("ERROR: invalid clock file extension selector")
        sys.exit(-1)

    Name = Design + Extension

    P = os.path.join(Default_CF_Dir, Chip, Name)
    if os.path.isfile(P):
        return P
    P = os.path.join(Custom_CF_Dir, Name)
    if os.path.isfile(P):
        return P

    return None


#
# Remove_Boot_Clock_File — Delete Clock_Dir + Clock_Name + "_boot" if it exists (restoreclock always).
#
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


#
# Create_Boot_Clock_File — For setbootclock when no .bin exists: write Clock_Design to
# Clock_Dir + Clock_Name + "_boot" (a custom clock design with no EEPROM image). If that file
# already exists, it is truncated and rewritten so the boot design always matches Clock_Design.
#
def Create_Boot_Clock_File(Clock_Name, Clock_Design):
    os.makedirs(Clock_Dir, exist_ok=True)
    Path = Clock_Dir + Clock_Name + "_boot"
    Content = Clock_Design + "\n"
    try:
        if os.path.isfile(Path):
            try:
                with open(Path, encoding="utf-8") as F:
                    if F.read() == Content:
                        return
            except OSError:
                pass
        # "w" creates the file or replaces an existing boot clock file in full.
        with open(Path, "w", encoding="utf-8") as F:
            F.write(Content)
        if hasattr(os, "sync"):
            os.sync()
    except OSError as E:
        print("ERROR: failed to write '%s': %s" % (Path, E))
        sys.exit(-1)


#
# main — Vendor script entry (sc_app argv: Board, bus, addr, default design, command, clock name, …).
#
# argv: Board I2C_Bus I2C_Addr Default_Design Command Clock_Name [Clock_Design ...]
# Clock_Design: argv[7] onward joined with spaces (one token or split .tcs/.txt/.bin list).
# setclock: Find_Clock_File + Program_Clock.
# setbootclock: Program_Clock; then Program_EEPROM if .bin found; else Create_Boot_Clock_File.
# restoreclock: Program_Clock; optional Program_EEPROM if default .bin exists; always Remove_Boot_Clock_File
# (clears setbootclock no-.bin marker; vendor_clock record cleared at boot in Boot_Set_Clocks).
#
def main():
    Num_Args = len(sys.argv) - 1
    if Num_Args < 6:
        print("ERROR: missing the required number of arguments")
        sys.exit(-1)

    Chip = os.path.basename(sys.argv[0].strip(".py"))
    Board = sys.argv[1]
    I2C_Bus = int(sys.argv[2].replace("/dev/i2c-", ""))
    I2C_Addr = int(sys.argv[3], 0)
    Default_Design = Clock_Design_Basename(sys.argv[4])
    Command = sys.argv[5]
    Clock_Name = sys.argv[6]

    if Command in ("setclock", "setbootclock"):
        if Num_Args < 7:
            print("ERROR: invalid number of arguments")
            sys.exit(-1)

        Clock_Design = Clock_Design_Basename(" ".join(sys.argv[7:]))

    if Command == "getclock":
        Discover_Clock(I2C_Bus, I2C_Addr, Chip, Board, Clock_Name, Default_Design)

    elif Command == "setclock":
        Runtime_File1 = Find_Clock_File(Chip, Clock_Design, Runtime_Ext1)
        Runtime_File2 = Find_Clock_File(Chip, Clock_Design, Runtime_Ext2)
        if not Runtime_File1 or not Runtime_File2:
            print("ERROR: failed to find design clock file '" + Clock_Design + "'")
            sys.exit(-1)

        Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File2)

    elif Command == "setbootclock":
        Runtime_File1 = Find_Clock_File(Chip, Clock_Design, Runtime_Ext1)
        Runtime_File2 = Find_Clock_File(Chip, Clock_Design, Runtime_Ext2)
        if not Runtime_File1 or not Runtime_File2:
            print("ERROR: failed to find design clock file '" + Clock_Design + "'")
            sys.exit(-1)

        Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File2)

        EEPROM_File = Find_Clock_File(Chip, Clock_Design, EEPROM_Ext)
        if EEPROM_File:
            RC = Program_EEPROM(I2C_Bus, I2C_Addr, EEPROM_File)
            if RC != 0:
                sys.exit(RC)
        else:
            Create_Boot_Clock_File(Clock_Name, Clock_Design)

    elif Command == "restoreclock":
        Runtime_File1 = Find_Clock_File(Chip, Default_Design, Runtime_Ext1)
        Runtime_File2 = Find_Clock_File(Chip, Default_Design, Runtime_Ext2)
        if not Runtime_File1 or not Runtime_File2:
            print("ERROR: failed to find design clock file '" + Default_Design + "'")
            sys.exit(-1)

        Program_Clock(I2C_Bus, I2C_Addr, Clock_Name, Runtime_File2)

        EEPROM_File = Find_Clock_File(Chip, Default_Design, EEPROM_Ext)
        if EEPROM_File:
            RC = Program_EEPROM(I2C_Bus, I2C_Addr, EEPROM_File)
            if RC != 0:
                sys.exit(RC)

        Remove_Boot_Clock_File(Clock_Name)

    else:
        print("ERROR: invalid clock command")
        sys.exit(-1)


if __name__ == "__main__":
    main()
