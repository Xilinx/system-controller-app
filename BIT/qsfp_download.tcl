#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect

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

set image_id [lindex $argv 1]
set image_uid [lindex $argv 2]

set uid_reg [unique_id $image_id]
if {$image_uid != $uid_reg} {
    switch_to_jtag
    puts "Loading $pdi"
    device program $pdi
} else {
    puts "PDI already loaded"
}

disconnect
