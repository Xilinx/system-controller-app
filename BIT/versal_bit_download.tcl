#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

set PGG1 0xf1110054
set PGG3 0xf111005C

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

versal_connect
# XXX- need to revisit this workaround when full labtools support is available for T50.
if {[string length [targets -nocase -filter {name =~ "*A78*"}]] != 0} {
    rst -system
}

set board [lindex $argv 0]
set testBitIdx [lindex $argv 3]
set action [expr {$testBitIdx >> 8} & 0xFF]

if {$action == 0 || $action == 1} {
	# Download the default PDI
	load_default_pdi [lindex $argv 1] [lindex $argv 2]

	apu_connect

	# Download the ELF file to run it on APU
	rst -clear-registers -skip-activate-subsystem -processor
	set elf $board
	append elf "/" versal_bit.elf
	dow $elf
	set sock [jtagterminal -start -socket]
	exec nc localhost $sock &
	con
	after 1000
}

if {$action == 0 || $action == 2} {
	apu_connect

	set sock [jtagterminal -start -socket]
	exec nc localhost $sock &

	# Set BIT test index
	mwr $PGG1 [expr {1 << (($testBitIdx & 0xff) - 1)}]

	# Wait for the test status cleared (= 0)
	variable stat 0xFFFFFFFF

	for {set i 0} {$i < 10} {incr i} {
		scan [lindex [mrd $PGG3] 1] "%x" ::stat
		if {$::stat == 0} {
			break
		}
		after 200
	}

	# Check for the test completed status (0x80000XXX)
	if {$::stat == 0} {
		for {set i 0} {$i < 100} {incr i} {
			after 1000
			scan [lindex [mrd $PGG3] 1] "%x" ::stat
			if {($::stat & 0x80000000) != 0} {
				break
			}
		}
	}

	# Check test passed or failed
	puts ""
	if {$::stat == 0x80000000} {
		puts "PASS"
	} else {
		puts "FAIL"
	}
}

disconnect
