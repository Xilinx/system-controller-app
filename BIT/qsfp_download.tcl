#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
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

# Get the IDCODE
set idcode_reg [mrd -force 0xF11A0000]
set idcode_str [lindex [split $idcode_reg ":"] 1]
set idcode_str [string map {" " ""} $idcode_str]
set idcode_str [string map {"\n" ""} $idcode_str]
set idcode [expr 0x$idcode_str]

# Determine silicon revision
#
# IDCODE[11:0] = 0x93   // Xilinx Manufacturer
set mask [expr 0xFFF]
if {($idcode & $mask) != 0x93} {
   puts "ERROR: invalid manufacturer"
   disconnect
   exit -1
}

# IDCODE[31:28]         // Silicon Revision
set revision [expr $idcode >> 28]
if {$revision == 0} {
   set revision_str "es1_"
} elseif {$revision == 1} {
   set revision_str ""
} else {
   puts "ERROR: unsupported revision"
   disconnect
   exit -1
}

# Download the PDI file
append pdi $revision_str "system_wrapper.pdi"
device program $pdi

disconnect
