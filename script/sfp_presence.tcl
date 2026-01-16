#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set dut [device_under_test]
if { $dut != "versal" } {
    puts "ERROR: unsupported $dut device-under-test"
    disconnect
    exit -1
}

set silicon [lindex [targets -nocase -filter {name =~ "*Versal*"}] 2]
jtag targets -set -filter {name == $silicon}

# Boundary Scan position
set position [lindex $argv 0]

# Get the Boundary Scan
set pins [bscan]

puts [string index $pins $position]

disconnect
