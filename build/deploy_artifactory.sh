#! /bin/bash

#
# Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

while getopts "d" OPTS; do
	case "$OPTS" in
		d)
			DRY_RUN="--dry-run"
			;;
		\?)
			echo "Usage: $0 [-d]"
			exit
			;;
	esac
done

RELEASE="2024.2"
BASE_URL="system-controller/sc_app_bsp/$RELEASE"

INTERNAL=( \
	"VE-P-A2112-00" \
	"VE-X-A2112-00" \
	"VM-P-M1369-00" \
	"VN-P-B2197-00" \
	"VPK180-112" \
	"VR-R-A2488-01" \
)

get_latest() {
	jf rt search --recursive=false --sort-by=name --sort-order=desc --include-dirs --limit=1 "$BASE_URL"/ 2>/dev/null | python3 -c 'import sys, json; print(json.load(sys.stdin)[0]["path"])'
}

mirror_release () {
	DATE_DIR="${RELEASE}_$(date +%Y%m%d%H%M)"

	#Find the most recent artifacts
	COPY_DIR=$1
	NEW_PROJ="$BASE_URL"/"$DATE_DIR"

	#Copying the artifacts works fine, but it doesn't update the directory creation date
	echo ">>> jf rt copy ${DRY_RUN} --flat=true ${COPY_DIR} ${NEW_PROJ}"
	jf rt copy ${DRY_RUN} --flat=true "${COPY_DIR}" "${NEW_PROJ}"
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

	echo ">>> jf rt upload ${DRY_RUN} ${J}.json ${COPY_DIR}/${DIR}/${J}/${J}.json"
	jf rt upload ${DRY_RUN} "${J}.json" "${COPY_DIR}"/"${DIR}"/"${J}"/"${J}.json"
done
