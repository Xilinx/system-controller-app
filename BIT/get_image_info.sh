#!/bin/sh
#
# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
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

read -r image_id unique_id <<<$(bootgen -arch versal -read $PDI | grep -A1 "name (0x10) : " | grep -A1 -E "pl_cfi|CONFIG_MASTER" | tail -n 1 | awk '{print $4 " " $8}')

echo $image_id $unique_id
