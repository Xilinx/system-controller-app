#! /bin/bash

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

PDI=$1

if [ "$PDI" == "" ]; then
   echo "PDI file not specified"
   exit -1
fi

if [ ! -f $PDI ]; then
   echo $PDI file not found
   exit -1
fi

for arch in versal versal_2ve_2vm; do
   read -r image_id unique_id <<<$(bootgen -arch $arch -read $PDI 2>/dev/null | grep -A1 "name (0x10) : " | grep -A1 -E "pl_cfi|CONFIG_MASTER" | tail -n 1 | awk '{print $4 " " $8}')
   [ -n "$image_id" ] && break
done

echo $image_id $unique_id
