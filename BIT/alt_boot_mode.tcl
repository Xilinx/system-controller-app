#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set dut [device_under_test]
dut_connect $dut

set alt_boot_mode [lindex $argv 0]
switch_bootmode $dut $alt_boot_mode

disconnect
