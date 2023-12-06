#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"


connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

# Get the IDCODE
set idcode [read_reg 0xF11A0000]

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
set pdi $revision_str
append pdi "system_wrapper.pdi"

set image_info [exec /usr/share/system-controller-app/BIT/get_image_info.sh $pdi]
set image_id [lindex $image_info 0]
set image_uid [lindex $image_info 1]

set uid_reg [unique_id $image_id]
if {$image_uid != $uid_reg} {
    switch_to_jtag
    puts "Loading $pdi"
    device program $pdi
} else {
    puts "PDI already loaded"
}

disconnect
