#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect

# Download the default PDI
load_default_pdi [lindex $argv 0] [lindex $argv 1]

# If the proc is 'load_default_pdi', we are done
if {[lindex $argv 2] eq "load_default_pdi"} {
    exit
}

# Invoke the proc
puts [[lindex $argv 2] [lrange $argv 3 end]]

disconnect
