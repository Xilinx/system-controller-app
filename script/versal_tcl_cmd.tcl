#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect

# Download the default PDI
load_default_pdi [lindex $argv 0] [lindex $argv 1] [lindex $argv 2]

# Invoke the proc
puts [[lindex $argv 3] [lrange $argv 4 end]]

disconnect
