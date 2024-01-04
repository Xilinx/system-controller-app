#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

set retcode 0
set revision_str [silicon_revision]

set mode [lindex $argv 0]
if {$mode != 0 && $mode != 1} {
   puts "ERROR: invalid argument"
   disconnect
   exit -1
}

#
# If 'mode' is set to 0, output either 'ES1' or 'PROD' as the revision of
# silicon.  If 'mode' is set to 1, output 'PASS' if silicon is either
# 'ES1' or 'PROD', otherwise output 'FAIL'.
#
if {$mode == 0} {
   if {$revision_str == "es1_"} {
      puts "ES1"
   } elseif {$revision_str == ""} {
      puts "PROD"
   } elseif {$revision_str == "invalid"} {
      puts "ERROR: unsupported silicon revision"
      set retcode -1
   }
} elseif {$mode == 1} {
   if {$revision_str == "invalid"} {
      puts "FAIL"
   } else {
      puts "PASS"
   }
}

disconnect
exit $retcode
