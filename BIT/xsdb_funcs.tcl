#!/usr/local/xilinx_vitis/xsdb
#
# Copyright (c) 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# XSDB helper functions for reading versal registers and various JTAG operations

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

# Return silicon revision string based on the IDCODE
proc silicon_revision {} {
   # Get the IDCODE
   set idcode [read_reg 0xF11A0000]

   # Determine silicon revision
   #
   # IDCODE[11:0] = 0x93   // Xilinx Manufacturer
   set mask [expr 0xFFF]
   if {($idcode & $mask) != 0x93} {
      revision_str "invalid"
      return $revision_str
   }

   # IDCODE[31:28]         // Silicon Revision
   set revision [expr $idcode >> 28]
   if {$revision == 0} {
      set revision_str "es1_"
   } elseif {$revision == 1} {
      set revision_str ""
   } else {
      set revision_str "invalid"
   }

   return $revision_str
}
