#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set board [lindex $argv 0]

if { $board == "" } {
    puts "ERROR: board name is not provided"
    exit -1
}

jtag_ready

# Select the JTAG target for the DUT.
set dut [device_under_test]
if { $dut != "versal" && $dut != "spartanup" } {
    puts "ERROR: unsupported $dut device-under-test"
    disconnect
    exit -1
}

dut_connect $dut

# Line 2 of silicon file: silicon revision.
set revision_str [silicon_revision]
if { $revision_str == "es1_" } {
    set revision_str "ES1"
} elseif { $revision_str == "" } {
    set revision_str "PROD"
} else {
    set revision_str "N/A"
}

if { $board == "VEK385" || $board == "VEK386" } {
    set crc [read_reg 0xF125023C]
    if { $crc == 0 } {
        set revision_str "ES1"
    } else {
        set revision_str "PROD"
    }
} elseif { $board == "SCU200" } {
    set revision_str "N/A"
}

puts "Silicon Revision: $revision_str"

# Line 3 of silicon file: device IDCODE.
set idcode_str [lindex [jtag ta -filter {idcode =~ "*093"}] 3]
if { ![string match 0x* $idcode_str] } {
    set idcode_str 0x$idcode_str
}

puts "Silicon IDCODE: $idcode_str"

# Line 4 of silicon file: device DNA.
set dna_line [lindex [split [device status -hex dna] "\n"] 0]
set dna_str [string trim [string range $dna_line 5 end]]

puts "Silicon DNA: $dna_str"

disconnect
exit 0
