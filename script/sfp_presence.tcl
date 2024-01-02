#
# Copyright (c) 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

set silicon [lindex [targets -nocase -filter {name =~ "*Versal*"}] 2]
jtag targets -set -filter {name == $silicon}

# Boundary Scan position
set position [lindex $argv 0]

# Get the Boundary Scan
set pins [bscan]

puts [string index $pins $position]

disconnect
