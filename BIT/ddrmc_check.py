#
# Copyright (c) 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import sys
import time
import subprocess
from chipscopy import create_session

if len(sys.argv) != 3:
    print("ERROR: missing DDRMC number")
    quit(-1)

DDRMC = int(sys.argv[1])
BOARD_NAME = str(sys.argv[2])

# Specify locations of the running hw_server and cs_server below
CS_URL = os.getenv("CS_SERVER_URL", "TCP:localhost:3042")
HW_URL = os.getenv("HW_SERVER_URL", "TCP:localhost:3121")

# Read the Versal UUID registers
def get_unique_id (versal_device, image_id):
    # Read the ImageInfoTable Address
    ImageInfoTableAddr = 0xF2014040
    offset = versal_device.memory_read(ImageInfoTableAddr)[0]
    if (offset == 0xdeadbeef or offset == 0):
        return 0xdeadbeef

    # Get the length of the ImageInfo Table
    length = versal_device.memory_read(ImageInfoTableAddr + 8)[0] & 0xffff

    # Parse the image info table until we find the requested image_id and return the UUID
    img = 0
    while img < length:
        image_id_reg = versal_device.memory_read(offset)[0]
        if image_id_reg == image_id:
            return versal_device.memory_read(offset + 4)[0]
        img += 1
        offset = offset + 16

    return 0xdeadbeef

with create_session(cs_server_url=CS_URL, hw_server_url=HW_URL) as session:
    check_for_device = True
    start_time = time.time()
    while check_for_device:
        try:
            versal_device = session.devices.filter_by(family="versal").get()
            check_for_device = False
        except ValueError:
            if time.time() - start_time > 5:
                print("ERROR: no device found on the JTAG chain")
                quit(-1)

    device_properties = versal_device.chipscope_node.props
    IDCODE = device_properties['idcode']

    # IDCODE[11:0] = 0x93   // Xilinx Manufacturer
    if hex(IDCODE & 0xFFF) != hex(0x93):
        print("ERROR: invalid manufacturer")
        quit(-1)

    # IDCODE[31:28]         // Silicon Revision
    Revision = (IDCODE >> 28)
    if Revision == 0:
        Revision_Str = "es1_"
    elif Revision == 1:
        Revision_Str = ""
    else:
        print("ERROR: unsupported revision")
        quit(-1)

    PDI_FILE = BOARD_NAME + "/" + Revision_Str + "system_wrapper.pdi"

    result = subprocess.check_output(['/usr/share/system-controller-app/BIT/get_image_info.sh', PDI_FILE ]).decode("utf-8")
    result = result.split()
    image_id = int(result[0], 0)
    image_uid = int(result[1], 0)

    unique_id = get_unique_id(versal_device, image_id)

    if (image_uid != unique_id):
        # Program the PDI
        versal_device.program(PDI_FILE)
    else:
        print("PDI already loaded")

    # Discover debug cores
    versal_device.discover_and_setup_cores()

    # Get the target DDRMC
    DDR = versal_device.ddrs[(DDRMC - 1)]
    if not DDR.is_enabled:
        print("ERROR: DDRMC " + str(DDRMC) + " is not enabled")
        quit(-1)

    print("Calibration Status:", DDR.get_cal_status())
