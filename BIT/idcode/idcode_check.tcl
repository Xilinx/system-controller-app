connect -xvc-url TCP:127.0.0.1:2542

targets -set -nocase -filter {name =~ "*Versal*"}

set idcode_reg [mrd -force 0xF11A0000]
# Remove the unwanted information from idcode_reg srting
set idcode [lindex [split $idcode_reg ":"] 1]
set idcode [string map {" " ""} $idcode]
set idcode [string map {"\n" ""} $idcode]

if {$idcode == "14CA8093"} {
   # VCK190: Versal xcvc1902
   puts "PASS"
} elseif {$idcode == "14CAA093"} {
   # VMK180: Versal xcvm1802
   puts "PASS"
} else {
   puts "ERROR: invalid idcode 0x$idcode"
}

disconnect
