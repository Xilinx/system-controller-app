#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

set PGG1 0xf1110054
set PGG3 0xf111005C

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

set board [lindex $argv 0]
set revision_str [silicon_revision]
if {$revision_str == "invalid"} {
    puts "ERROR: unsupported silicon revision"
    disconnect
    exit -1
}

# Download the PDI file
set pdi $board
append pdi "/" $revision_str "system_wrapper.pdi"

set image_info [exec /usr/share/system-controller-app/BIT/get_image_info.sh $pdi]
set image_id [lindex $image_info 0]
set image_uid [lindex $image_info 1]

set uid_reg [unique_id $image_id]
if {$image_uid != $uid_reg} {
    switch_to_jtag
    puts "Loading $pdi"
    device program $pdi
} else {
    puts "PDI already loaded"
}

# Download the ELF file and run it on APU
targets -set -nocase -filter {name =~ "*A72*0"}
rst -clear-registers -skip-activate-subsystem -processor
set elf $board
append elf "/" versal_bit.elf
dow $elf
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
