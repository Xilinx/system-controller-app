#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

proc switch_to_jtag {} {
   # Enable ISO
   mwr -force 0xf1120000 0xffbff

   # Switch to JTAG boot mode
   mwr -force 0xf1260200 0x0100

   # Set Multi-boot address to 0
   mwr -force 0xF1110004 0x0

   # SYSMON_REF_CTRL is switched to NPI by user PDI so ensure its
   # switched back
   mwr -force 0xF1260138 0
   mwr -force 0xF1260320 0x77

   # Perform reset
   rst -system
}

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}
switch_to_jtag

set idcode_reg [mrd -force 0xF11A0000]
# Remove the unwanted information from idcode_reg srting
set idcode [lindex [split $idcode_reg ":"] 1]
set idcode [string map {" " ""} $idcode]
set idcode [string map {"\n" ""} $idcode]

cd /usr/share/system-controller-app/BIT/qspi

# Based on IDCODE download the correct PDI
if {$idcode == "14CA8093"} {
   device program vck190_system_wrapper.pdi
} elseif {$idcode == "14CAA093"} {
   device program vmk180_system_wrapper.pdi
} elseif {$idcode == "04D00093"} {
   device program vpk120_es1_system_wrapper.pdi
} else {
   puts "Invalid IDCODE!"
}

disconnect
