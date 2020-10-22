connect -xvc-url TCP:127.0.0.1:2542

targets -set -nocase -filter {name =~ "*Versal*"}

set idcode_reg [mrd -force 0xF11A0000]
# Remove the unwanted information from idcode_reg srting
set idcode [lindex [split $idcode_reg ":"] 1]
set idcode [string map {" " ""} $idcode]
set idcode [string map {"\n" ""} $idcode]

cd /usr/share/system-controller-app/BIT/qsfp_set_modsel

# Based on IDCODE download the correct PDI
if {$idcode == "04CA8093"} {
   device program vck190_es1_system_wrapper.pdi
} elseif {$idcode == "14CA8093"} {
   device program vck190_system_wrapper.pdi
} elseif {$idcode == "04CAA093"} {
   device program vmk180_es1_system_wrapper.pdi
} elseif {$idcode == "14CAA093"} {
   puts "No PDI is available!"
} else {
   puts "Invalid IDCODE!"
}

disconnect
