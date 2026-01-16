#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set dut [device_under_test]
dut_connect $dut

# Check whether device-under-test is idle
if {[check_done] == 1} {
    puts "DUT is running a PDI, assert reset and retry"
    disconnect
    exit -1
}

# Download the PDI file
set pdi [lindex $argv 0]

if { $dut == "versal" } {
    switch_bootmode $dut 0
} elseif { $dut == "spartanup" } {
    switch_bootmode $dut 5
} else {
    puts "ERROR: failed to set bootmode to JTAG"
    disconnect
    exit -1
}

puts "Loading $pdi"
device program $pdi
if { $dut != "spartanup" } {
    print_banner
}

disconnect
