#! /usr/local/xilinx_vitis/xsdb

#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
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

disconnect
