#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

connect -xvc-url TCP:127.0.0.1:2542

targets -set -nocase -filter {name =~ "*Versal*"}

set idcode_reg [mrd -force 0xF11A0000]
# Remove the unwanted information from idcode_reg srting
set idcode [lindex [split $idcode_reg ":"] 1]
set idcode [string map {" " ""} $idcode]
set idcode [string map {"\n" ""} $idcode]

if {$idcode == "14CA8093"} {
   puts "XCVC1902 PROD"
} elseif {$idcode == "04CA8093"} {
   puts "XCVC1902 ES1"
} elseif {$idcode == "14CAA093"} {
   puts "XCVM1802 PROD"
} elseif {$idcode == "04CAA093"} {
   puts "XCVM1802 ES1"
} elseif {$idcode == "14D00093"} {
   puts "XCVP1202 PROD"
} elseif {$idcode == "04D00093"} {
   puts "XCVP1202 ES1"
} elseif {$idcode == "14D14093"} {
   puts "XCVP1802 PROD"
} elseif {$idcode == "04D14093"} {
   puts "XCVP1802 ES1"
} else {
   puts "ERROR: invalid idcode 0x$idcode"
}

disconnect
