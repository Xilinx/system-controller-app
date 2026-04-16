#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

source "/usr/share/system-controller-app/BIT/xsdb_funcs.tcl"

set sock ""
set board [lindex $argv 0]
set testBitIdx [lindex $argv 3]
set action [expr {$testBitIdx >> 8} & 0xFF]

set dut [device_under_test]
if {$dut == "versal"} {
    set PGG1 0xF1110054
    set PGG3 0xF111005C
    set GGS3 0xF111003C
    set GGS4 0xF1110040
} elseif {$dut == "spartanup"} {
    set PGG1 0x040A00C4
    set PGG3 0x040A00CC
    set GGS3 0x040A00AC
    set GGS4 0x040A00B0
} else {
    puts "ERROR: unsupported $dut device-under-test"
    disconnect
    exit -1
}

if {$action == 0 || $action == 1} {
    # Select 'device-under-test' target to load the default PDI
    dut_connect $dut

    # XXX- need to revisit this workaround when full labtools support is available for T50.
    if {[string length [targets -nocase -filter {name =~ "*A78*"}]] != 0} {
        rst -system
    }

    # Download the default PDI
    load_default_pdi $dut [lindex $argv 1] [lindex $argv 2]

    if {$dut == "versal"} {
        apu_connect
        rst -clear-registers -skip-activate-subsystem -processor
    } elseif {$dut == "spartanup"} {
        spartanup_connect
        stop
    }

    # Download the ELF binary to run on processor
    set elf $board
    append elf "/" versal_bit.elf
    dow $elf

    # Connect to 'jtagterminal'
    if {$dut == "versal"} {
        set sock [jtagterminal -start -socket]
        exec nc localhost $sock &
    }

    # Run the elf binary
    con
    after 1000

    set status [sc_to_dut_data_transfer $GGS3 $GGS4 $board]
    if {$status != 0} {
        puts "ERROR: failed to transfer board name"
    }

    after 100
}

if {$action == 0 || $action == 2} {
    if {$dut == "versal"} {
        apu_connect

        # Re-connect to 'jtagterminal', if it is not already connected
        if { $sock == "" } {
            set sock [jtagterminal -start -socket]
            exec nc localhost $sock &
        }
    } else {
        spartanup_connect
    }

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
        # BIT index 6 is 'PL UART Test' and index 7 is 'LEDs Test'; The PASS
        # status for these tests can not be determined by the code itself
        # and it requires visual inspection by the user.
        if {($testBitIdx & 0xff) == 6 || ($testBitIdx & 0xff) == 7} {
            puts "COMPLETE"
        } else {
            puts "PASS"
        }
    } else {
        puts "FAIL"
    }
}

disconnect
