#! /usr/bin/env python3

#
# Copyright (c) 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
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

if len(sys.argv) != 5:
    print("ERROR: missing DDRMC number, board name, image id, and unique id")
    quit(-1)

DDRMC = int(sys.argv[1])
BOARD_NAME = str(sys.argv[2])
image_id = int(sys.argv[3], 16)
image_uid = int(sys.argv[4], 16)

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

with create_session(cs_server_url=CS_URL, hw_server_url=HW_URL, pre_device_scan_delay=3000) as session:
    # XXX- a workaround for early release of labtools binaries for T50
    if BOARD_NAME == "VEK385":
        session = create_session(cs_server_url=CS_URL, hw_server_url=HW_URL)

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

    PDI_FILE = "/data/PDIs/default.pdi"

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
