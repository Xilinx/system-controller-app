#
# Copyright (c) 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

# Check whether Versal is idle
if {[check_done] == 1} {
    puts "Versal is running a PDI, assert reset and retry"
    exit -1
}

# Download the PDI file
set pdi [lindex $argv 0]

switch_to_jtag
puts "Loading $pdi"
device program $pdi

disconnect
