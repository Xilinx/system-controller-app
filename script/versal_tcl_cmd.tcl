#! /usr/local/xilinx_vitis/xsdb

#
# Copyright (c) 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

# Download the default PDI
load_default_pdi [lindex $argv 0]

# Invoke the proc
puts [[lindex $argv 1] [lrange $argv 2 end]]

disconnect
