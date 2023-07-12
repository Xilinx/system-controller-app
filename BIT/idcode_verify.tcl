#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

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
   puts "ES1"
} elseif {$revision == 1} {
   puts "PROD"
} else {
   puts "ERROR: unsupported revision"
   disconnect
   exit -1
}

disconnect
