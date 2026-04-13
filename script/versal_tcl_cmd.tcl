#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2024 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set dut [device_under_test]
dut_connect $dut

# Download the default PDI
load_default_pdi $dut [lindex $argv 0] [lindex $argv 1]

# If the proc is 'load_default_pdi', we are done
if {[lindex $argv 2] eq "load_default_pdi"} {
    exit
}

# For Versal Gen2 retarget so PL's address space could be accessed from xsdb
if {[catch {targets -set -nocase -filter {name =~ "*A78*#0.0"}} err] == 0} {
    if {[string length [targets -nocase -filter {name =~ "*A78*#0.0" && state_reason =~ "Power On Reset"}]] != 0} {
        rst -clear-registers -skip-activate-subsystem -processor
    }
}

# For Spartan UltraScale+ re-target so PL's address space could be accessed from xsdb
if { $dut == "spartanup" } {
    spartanup_connect
}

# Invoke the proc
puts [[lindex $argv 2] [lrange $argv 3 end]]

disconnect
