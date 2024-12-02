#! /bin/bash

#
# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

RELEASE="2024.2"
BASE_URL="system-controller/sc_app_bsp/$RELEASE"
#OPTS="--dry-run"

INTERNAL=( \
	"VN-P-B2197-00" \
	"VPK180-112" \
	"VR-R-A2488-00" \
	"VE-P-A2112-00" \
)

get_latest() {
	jf rt search --recursive=false --sort-by=name --sort-order=desc --include-dirs --limit=1 "$BASE_URL"/ 2>/dev/null | python3 -c 'import sys, json; print(json.load(sys.stdin)[0]["path"])'
}

mirror_release () {
	DATE_DIR="${RELEASE}_$(date +%m%d%y%H%M)"

	#Find the most recent artifacts
	COPY_DIR=$1
	NEW_PROJ="$BASE_URL"/"$DATE_DIR"

	#Copying the artifacts works fine, but it doesn't update the directory creation date
	jf rt copy $OPTS --flat=true "$COPY_DIR" "$NEW_PROJ"
}

COPY_DIR=$(get_latest)

mirror_release "$COPY_DIR"

COPY_DIR=$(get_latest)

cd ../board || exit

for J in $(ls *.json | sed 's/\.json//'); do 
	DIR="external"
	for I in "${INTERNAL[@]}"; do
		if [ "$J" == "$I" ]; then
			DIR="internal"
		fi
	done

	jf rt upload $OPTS "$J.json" "$COPY_DIR"/"$DIR"/"$J"/"$J.json"
done
