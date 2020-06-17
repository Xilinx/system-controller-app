connect -xvc-url TCP:127.0.0.1:2542

targets -set -nocase -filter {name =~ "*Versal*"}

set idcode_reg [mrd -force 0xF11A0000]
# Remove the unwanted information from idcode_reg srting
set idcode [lindex [split $idcode_reg ":"] 1]
set idcode [string map {" " ""} $idcode]
set idcode [string map {"\n" ""} $idcode]

# Check with IDCODE of xcvc1902 device
if { $idcode == "04CA8093"} {
   puts "PASS"
} else {
   puts "FAIL" 
}

disconnect
