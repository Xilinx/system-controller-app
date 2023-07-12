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
from chipscopy import create_session

if len(sys.argv) != 3:
    print("ERROR: missing DDRMC number")
    quit(-1)

DDRMC = int(sys.argv[1])
BOARD_NAME = str(sys.argv[2])

# Specify locations of the running hw_server and cs_server below
CS_URL = os.getenv("CS_SERVER_URL", "TCP:localhost:3042")
HW_URL = os.getenv("HW_SERVER_URL", "TCP:localhost:3121")

with create_session(cs_server_url=CS_URL, hw_server_url=HW_URL) as session:
    time.sleep(5)
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

    PDI_FILE = Revision_Str + "system_wrapper.pdi"
    # Program the PDI
    versal_device.program(BOARD_NAME + "/" + PDI_FILE)

    # Discover debug cores
    versal_device.discover_and_setup_cores()

    # Get the target DDRMC
    DDR = versal_device.ddrs[(DDRMC - 1)]
    if not DDR.is_enabled:
        print("ERROR: DDRMC " + str(DDRMC) + " is not enabled")
        quit(-1)

    print("Calibration Status:", DDR.get_cal_status())
