#
# Copyright 2022 Xilinx, Inc.
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
import time
from chipscopy import create_session

DDRMC = 2
BIT_Dir = "/usr/share/system-controller-app/BIT"
DDRMC_Dir = BIT_Dir + "/ddrmc_" + str(DDRMC) + "/"

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

    if IDCODE == 0x14CAA093:
        PDI_FILE = "vmk180_ddr4_" + str(DDRMC - 1) + ".pdi"
    elif IDCODE == 0x14CA8093:
        PDI_FILE = "vck190_ddr4_" + str(DDRMC - 1) + ".pdi"
    elif IDCODE == 0x04D00093:
        PDI_FILE = "vpk120_es1_ddr4_" + str(DDRMC - 1) + ".pdi"
    elif IDCODE == 0x14D00093:
        PDI_FILE = "vpk120_ddr4_" + str(DDRMC - 1) + ".pdi"
    elif IDCODE == 0x04D14093:
        PDI_FILE = "vpk180_es1_ddr4_" + str(DDRMC - 1) + ".pdi"
    elif IDCODE == 0x14D14093:
        PDI_FILE = "vpk180_ddr4_" + str(DDRMC - 1) + ".pdi"
    else:
        print("ERROR: DDRMC test for idcode", hex(IDCODE), "is not supported")
        quit(-1)

    # Program the PDI
    versal_device.program(DDRMC_Dir + PDI_FILE)

    # Discover debug cores
    versal_device.discover_and_setup_cores()

    # Get the target DDRMC
    DDR = versal_device.ddrs[(DDRMC - 1)]
    if not DDR.is_enabled:
        print("ERROR: DDRMC " + str(DDRMC) + " is not enabled")
        quit(-1)

    print("Calibration Status:", DDR.get_cal_status())
