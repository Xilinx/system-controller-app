#! /usr/local/xilinx_vitis/xsdb

#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

set PGG1 0xf1110054
set PGG3 0xf111005C

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect

set board [lindex $argv 0]

# Download the default PDI
load_default_pdi $board

# Download the ELF file and run it on APU
targets -set -nocase -filter {name =~ "*A72*0"}
rst -clear-registers -skip-activate-subsystem -processor
set elf $board
append elf "/" versal_bit.elf
dow $elf
set sock [jtagterminal -start -socket]
exec nc localhost $sock &
con
after 1000

# Set BIT test index
mwr $PGG1 [lindex $argv 1]

# Wait for the test status cleared (= 0)
variable stat 0xFFFFFFFF

for {set i 0} {$i < 10} {incr i} {
    scan [lindex [mrd $PGG3] 1] "%x" ::stat
    if {$::stat == 0} {
        break
    }
    after 200
}

# Check for the test completed status (0x80000XXX)
if {$::stat == 0} {
    for {set i 0} {$i < 100} {incr i} {
        after 1000
        scan [lindex [mrd $PGG3] 1] "%x" ::stat
        if {($::stat & 0x80000000) != 0} {
            break
        }
    }
}

# Check test passed or failed
if {$::stat == 0x80000000} {
    puts "PASS"
} else {
    puts "FAIL"
}

disconnect
