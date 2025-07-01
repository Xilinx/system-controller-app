#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect
# XXX- need to revisit this workaround when full labtools support is available for T50.
if {[string length [targets -nocase -filter {name =~ "*A78*"}]] != 0} {
    rst -system
}

# Download the default PDI
load_default_pdi [lindex $argv 0] [lindex $argv 1]

# If the proc is 'load_default_pdi', we are done
if {[lindex $argv 2] eq "load_default_pdi"} {
    exit
}

# T50 workaround in order for PL's address space could be accessed from xsdb
if {[catch {targets -set -nocase -filter {name =~ "*A78*#0.0"}} err] == 0} {
    rst -clear-registers -skip-activate-subsystem -processor
}

# Invoke the proc
puts [[lindex $argv 2] [lrange $argv 3 end]]

disconnect
