#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

# Enable ISO
mwr -force 0xf1120000 0xffbff

# Switch to alternate boot mode
set alt_boot_mode [lindex $argv 0]
set boot_mode_user 0x${alt_boot_mode}100
mwr -force 0xf1260200 $boot_mode_user

# Set Multi-boot address to 0
mwr -force 0xF1110004 0x0

# SYSMON_REF_CTRL is switched to NPI by user PDI so ensure its
# switched back
mwr -force 0xF1260138 0
mwr -force 0xF1260320 0x77

# Perform reset
rst -system

disconnect
