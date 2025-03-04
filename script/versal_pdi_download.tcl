#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect

# Check whether Versal is idle
if {[check_done] == 1} {
    puts "Versal is running a PDI, assert reset and retry"
    disconnect
    exit -1
}

# Download the PDI file
set pdi [lindex $argv 0]

switch_to_jtag
puts "Loading $pdi"
device program $pdi
print_banner

disconnect
