#!/opt/labtools/xilinx_vitis/xsdb

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

#
# XSDB helper functions for reading versal registers and various JTAG operations
#

# endian swap for a 32bit integer
proc swap32 {n} {
    return [expr [expr $n >> 24] | [expr [expr $n & 0x00ff0000] >> 8] | [expr [expr $n & 0x0000ff00] <<  8] | [expr [expr $n & 0x000000ff] << 24]]
}

# Read the DONE bit from the device status
proc check_done {} {
    set stat [device status jtag_status]
    set DONE_IDX [lsearch $stat DONE]
    return [lindex $stat [expr $DONE_IDX + 3]]
}

# read a register and return it as a proper integer value
proc read_reg {reg} {
    set reg_val [mrd -force $reg]
    set reg_str [lindex [split $reg_val ":"] 1]
    set reg_str [string map {" " ""} $reg_str]
    set reg_str [string map {"\n" ""} $reg_str]
    set reg_val [expr 0x$reg_str]
    return $reg_val
}

# Wait until register equals ack or complete, with timeout in ms
proc wait_for_state {reg ack complete timeout_ms} {
    set t $timeout_ms
    while {$t > 0} {
        set cur [read_reg $reg]
        if {$cur == $ack}      { return $ack }
        if {$cur == $complete} { return $complete }
        after 10
        incr t -10
    }
    return -1
}

proc sc_to_dut_data_transfer {ctrl_reg data_reg data_str} {
    set STATE_ACK       0x80
    set STATE_COMPLETE  0xFF
    set STATE_DATA      0x01

    set len    [string length $data_str]
    set chunks [expr {($len + 3) / 4}]

    # Wait for initial ACK
    if {[wait_for_state $ctrl_reg $STATE_ACK $STATE_COMPLETE 5000] != $STATE_ACK} {
        puts "ERROR: DUT is not ready"
        return -1
    }

    for {set c 0} {$c < $chunks} {incr c} {
        # Pack 4-byte chunk
        set word 0
        for {set i 0} {$i < 4} {incr i} {
            set idx [expr {$c * 4 + $i}]
            set ascii [expr {$idx < $len ? [scan [string index $data_str $idx] %c] : 0}]
            set word [expr {$word | (($ascii & 0xFF) << ($i * 8))}]
        }

        mwr $data_reg $word
        mwr $ctrl_reg $STATE_DATA

        # Wait for ACK or COMPLETE
        set s [wait_for_state $ctrl_reg $STATE_ACK $STATE_COMPLETE 5000]
        if {$s == $STATE_COMPLETE} {
            return 0
        } elseif {$s != $STATE_ACK} {
            puts "ERROR: Timeout after chunk $c"
            return -1
        }

    }
    return 0
}

# Run a Boundary Scan Sample command to get the state of all IO pins
proc bscan {} {
    set s [jtag seq]
    $s state RESET
    $s irshift -state IDLE -bits 14 11100000100000
    $s drshift -state IDLE -tdi 0 -capture 3848
    set r [$s run -bits]
    $s delete
    return $r
}

# Read the JTAG USERCODE register
proc usercode {} {
    set s [jtag seq]
    $s state RESET
    $s irshift -state IDLE -reg usercode
    $s drshift -state IDLE -tdi 0 -capture 32
    set r [$s run -int]
    $s delete
    return $r
    #return [string range [device status usercode] 10 20]
}

# Read JTAG user registers
proc user {} {
    set s [jtag seq]
    $s state RESET
    $s irshift -state IDLE -reg user1
    $s drshift -state IDLE -tdi 0 -capture 32
    $s irshift -state IDLE -reg user2
    $s drshift -state IDLE -tdi 0 -capture 32
    $s irshift -state IDLE -reg user3
    $s drshift -state IDLE -tdi 0 -capture 32
    $s irshift -state IDLE -reg user4
    $s drshift -state IDLE -tdi 0 -capture 32
    set user [$s run -int]
    $s delete
    return $user
}

# Read the JTAG IDCODE register using standard JTAG commands
proc idcode {} {
    set s [jtag seq]
    $s state RESET
    $s irshift -state IDLE -reg idcode
    $s drshift -state IDLE -tdi 0 -capture 32
    set r [$s run -hex]
    $s delete
    return $r
}

# Read the Versal UUID registers
proc unique_id {image_id} {
  # Read the ImageInfoTable Address
  set offset [read_reg 0xF2014040]
  if { $offset == "0xdeadbeef" || $offset == "0x00000000" } {
      return 0xdeadbeef
  }
  # Get the length of the ImageInfo Table
  set length [expr [read_reg 0xF2014048] & 0xffff]

  # Parse the image info table until we find the requested image_id and return the UUID
  set img 0
  while {$img < $length} {
    set image_id_reg [read_reg $offset]
    if {$image_id_reg == $image_id} {
        return [read_reg [expr $offset + 4]]
    }
    incr img
    set offset [expr $offset + 16]
  }
  return 0xdeadbeef
}

proc switch_bootmode {dut alt_boot_mode} {
   set boot_mode_user 0x${alt_boot_mode}100
   if { $dut == "versal" } {
       # Enable ISO
       mwr -force 0xf1120000 0xffbff

       # Switch to JTAG boot mode
       mwr -force 0xf1260200 $boot_mode_user

       # Set Multi-boot address to 0
       mwr -force 0xF1110004 0x0

       # SYSMON_REF_CTRL is switched to NPI by user PDI so ensure its
       # switched back
       mwr -force 0xF1260138 0
       mwr -force 0xF1260320 0x77
   } elseif { $dut == "spartanup" } {
       # Connect to PMC target
       spartanup_connect "PMC"

       # Switch to JTAG boot mode
       mwr -force 0x040A007C $boot_mode_user

       # Set Multi-boot address to 0
       mwr -force 0x040A0130 0x0
   } else {
       puts "ERROR: unable to switch_bootmode for $dut device-under-test"
   }

   # Perform reset
   srst $dut
}

# Return silicon revision string based on the IDCODE
proc silicon_revision {} {
   # Determine silicon revision
   #
   # IDCODE[11:0] = 0x093   // Xilinx Manufacturer
   set idcode_str [lindex [jtag ta -filter {idcode =~ "*093"}] 3]
   if {$idcode_str == ""} {
      return "unknown"
   }

   set prefix "0x"
   set idcode_val $prefix$idcode_str
   set mask [expr 0xFFF]
   if {($idcode_val & $mask) != 0x93} {
      return "invalid"
   }

   # IDCODE[31:28]         // Silicon Revision
   set revision [expr $idcode_val >> 28]
   if {$revision == 0} {
      set revision_str "es1_"
   } elseif {$revision == 1} {
      set revision_str ""
   } else {
      set revision_str "invalid"
   }

   return $revision_str
}

# Load the default PDI
proc load_default_pdi {dut image_id image_uid} {
    set pdi "/data/PDIs/default.pdi"
    if { $dut != "spartanup" } {
        set uid_reg [unique_id $image_id]
        set jtag_bootmode 0
    } else {
        set uid_reg -1
        set jtag_bootmode 5
    }

    if {$image_uid != $uid_reg} {
        switch_bootmode $dut $jtag_bootmode
        puts "Loading $pdi"
        device program $pdi
        if { $dut != "spartanup" } {
            print_banner
	}

    } else {
        puts "PDI already loaded"
    }
}

# Read clock counter and return it as a frequency value
proc read_clock {reg} {
    set ref_clk 100.0
    set sample_count 0x100000
    set clock_scale [expr $ref_clk / $sample_count]
    set int_val [read_reg $reg]
    set float_clock [expr $int_val * $clock_scale]
    return $float_clock
}

# Print a message on the console
proc print_console {uart0 message} {
    foreach char [split $message ""] {
        mw -force $uart0 [scan $char "%c"]
    }
}

proc print_banner {} {
    if {[string length [targets -filter {name =~ "*Versal Gen 2*"}]] != 0} {
        set uart0 0xF1920000
    } else {
        set uart0 0xFF000000
    }

    print_console $uart0 "\r\n"
    print_console $uart0 "***********************************************\r\n"
    print_console $uart0 "* Versal image is loaded by System Controller *\r\n"
    print_console $uart0 "***********************************************\r\n"
}

proc is_spartanup {idcode} {
    if { ($idcode & 0x0ff800ff) == 0x04e80093 } {
        return 1
    }

    return 0
}

# Used by 'spartanup' device-under-test
proc pmc_tap_id {} {
    set devices [jtag ta -ta -filter {level==1}]
    if { [llength $devices] == 0 } {
        error "scan chain has no devices"
    }

    set count 0
    set node {}
    foreach device $devices {
        set idcode 0x[xsdb::dict_get_safe $device idcode]
        set target_ctx [xsdb::dict_get_safe $device target_ctx]
        if { $target_ctx != "" && [is_spartanup $idcode] } {
            set node $target_ctx
            incr count
            if { [xsdb::dict_get_safe $device is_current] == 1 } {
                set count 1
                break
            }
        }
    }

    if { $count > 1 } {
        error "multiple targets found, please select one"
    }

    return $node
}

# System Reset
proc srst {dut} {
    if { $dut == "versal" } {
        rst -system
    } elseif { $dut == "spartanup" } {
        set node [pmc_tap_id]
        jtag targets -set -filter {target_ctx==$node}
        set s [jtag seq]
        #    $s state RESET
        $s irshift -state IDLE -int 6 0x37
        $s run -node $node
        after 1000
        $s clear
        #    $s state RESET
        $s irshift -state IDLE -int 6 0x3f
        $s run -node $node
        $s delete
    } else {
        puts "ERROR: unable to system reset $dut device-under-test"
    }

    return
}

# Wait for jtag targets to become accessible
proc jtag_ready {} {
    connect -xvc-url TCP:127.0.0.1:2542

    set retry 0
    while {$retry < 25} {
        if {[string first "closed" "[jtag targets]"] != -1} {
            after 100
            incr retry
        } else {
            break
        }
    }
}

# Determine which 'device-under-test' is in-use
proc device_under_test {} {
    jtag_ready

    if {[string length [targets -nocase -filter {name =~ "*versal*"}]] != 0} {
        return "versal"
    } elseif {[string length [targets -nocase -filter {name =~ "*xcsu200p*"}]] != 0} {
        return "spartanup"
    } else {
        puts "ERROR: unsupported device-under-test"
        return ""
    }
}

# Connect to 'device-under-test' target
proc dut_connect {dut} {
    if { $dut == "versal" } {
        targets -set -nocase -filter {name =~ "*Versal*"}
    } elseif { $dut == "spartanup" } {
        targets -set -nocase -filter {name =~ "*xcsu200p*"}
    } else {
        puts "ERROR: failed to connect to $dut device-under-test"
    }
}

# Connect to APU target
proc apu_connect {} {
    jtag_ready
    if {[catch {targets -set -nocase -filter {name =~ "*A72*0"}} err]} {
        if {[catch {targets -set -nocase -filter {name =~ "*A78*#0.0"}} err]} {
            puts "Failed to set target"
        }
    }
}

# Connect to targets on Spartan UltraScale+
proc spartanup_connect {module} {
    if { $module == "USER" } {
        set line [targets -nocase -filter {name =~ "*RISC-V at USER*"}]
    } elseif { $module == "PMC" } {
        set line [targets -nocase -filter {name =~ "*RISC-V at PMC*"}]
    } else {
        puts "ERROR: invalid target module"
        return
    }

    if { $line == "" } {
        device reset
        after 1000
        spartanup_connect $module
        return
    }

    set module_index [lindex $line 0]
    # The 'Hart' target is one after the 'RISC-V' target.
    set index [expr $module_index + 1]
    targets -set $index
}
