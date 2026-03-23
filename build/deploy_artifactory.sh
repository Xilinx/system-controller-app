#! /bin/bash

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

set -euo pipefail

trap 'echo "Error on line $LINENO (exit code $?)"' ERR

usage() {
	echo "Usage: $0 [-d] <release> <board_name> <target_file> <repo_path>"
	echo ""
	echo "  Copies the latest Artifactory release folder to a new timestamped"
	echo "  folder, then updates the board-specific tar with the target file(s)."
	echo ""
	echo "Options:"
	echo "  -d              Dry-run mode (no changes made)"
	echo ""
	echo "Arguments:"
	echo "  release         Release version (e.g. 2026.1)"
	echo "  board_name      Name of the board (e.g. vrk160)"
	echo "  target_file     Type of file to update:"
	echo "                    json  - board JSON config (board/{BOARD}.json)"
	echo "                    elf   - Versal bit ELF (BIT/{BOARD}/versal_bit.elf)"
	echo "                    js    - board strings JS (src/static/js/{board}_strings.js)"
	echo "                    img   - board images ({BOARD}_home.png, {board}.jpg)"
	echo "                    tar   - all allowed files from repo_path directory"
	echo "  repo_path       Local path to the repository (or directory for 'tar')"
	exit 1
}

DRY_RUN=""

while getopts "d" OPTS; do
	case "$OPTS" in
		d)
			DRY_RUN="--dry-run"
			;;
		\?)
			usage
			;;
	esac
done

shift $((OPTIND - 1))

if [ $# -ne 4 ]; then
	usage
fi

RELEASE="$1"
BOARD_NAME="$2"
TARGET_FILE="$3"
REPO_PATH="$4"

BASE_URL="system-controller/sc_app_bsp/$RELEASE"

RELEASE_CHECK=$(jf rt search --recursive=false --include-dirs --limit=1 "${BASE_URL}/" | \
	python3 -c 'import sys, json; r=json.load(sys.stdin); print(len(r))')
if [ "${RELEASE_CHECK}" -eq 0 ]; then
	echo "Error: release folder '${BASE_URL}' does not exist in Artifactory"
	exit 1
fi

BOARD_UPPER=$(echo "${BOARD_NAME}" | tr '[:lower:]' '[:upper:]')
TAR_NAME="systemcontroller-app-${BOARD_NAME}.tar.gz"

case "${TARGET_FILE}" in
	json)
		SOURCE_FILES=("${REPO_PATH}/board/${BOARD_UPPER}.json")
		TAR_FILE_NAMES=("${BOARD_UPPER}.json")
		for REV_FILE in "${REPO_PATH}"/board/${BOARD_UPPER}-[A-Z][0-9][0-9].json; do
			if [ -f "${REV_FILE}" ]; then
				SOURCE_FILES+=("${REV_FILE}")
				TAR_FILE_NAMES+=("$(basename "${REV_FILE}")")
			fi
		done
		;;
	elf)
		SOURCE_FILES=("${REPO_PATH}/BIT/${BOARD_UPPER}/versal_bit.elf")
		TAR_FILE_NAMES=("${BOARD_NAME}_versal_bit.elf")
		;;
	js)
		SOURCE_FILES=("${REPO_PATH}/src/static/js/${BOARD_NAME}_strings.js")
		TAR_FILE_NAMES=("${BOARD_NAME}_strings.js")
		;;
	img)
		SOURCE_FILES=(
			"${REPO_PATH}/src/static/images/${BOARD_UPPER}_home.png"
			"${REPO_PATH}/src/static/images/${BOARD_NAME}.jpg"
		)
		TAR_FILE_NAMES=(
			"${BOARD_UPPER}_home.png"
			"${BOARD_NAME}.jpg"
		)
		;;
	tar)
		if [ ! -d "${REPO_PATH}" ]; then
			echo "Error: directory '${REPO_PATH}' not found"
			exit 1
		fi

		REV_UPPER="[A-Z][0-9]{2}"
		REV_LOWER="[a-z][0-9]{2}"

		ALLOWED_PATTERN="^(LICENSE_BINARIES\.md"
		ALLOWED_PATTERN+="|${BOARD_UPPER}\.json"
		ALLOWED_PATTERN+="|${BOARD_UPPER}-${REV_UPPER}\.json"
		ALLOWED_PATTERN+="|${BOARD_UPPER}_home\.png"
		ALLOWED_PATTERN+="|ser2net_${BOARD_NAME}\.yaml"
		ALLOWED_PATTERN+="|ser2net_${BOARD_NAME}_${REV_LOWER}\.yaml"
		ALLOWED_PATTERN+="|${BOARD_NAME}\.jpg"
		ALLOWED_PATTERN+="|${BOARD_NAME}_es1_system_wrapper\.pdi"
		ALLOWED_PATTERN+="|${BOARD_NAME}_${REV_UPPER}_es1_system_wrapper\.pdi"
		ALLOWED_PATTERN+="|${BOARD_NAME}_system_wrapper\.pdi"
		ALLOWED_PATTERN+="|${BOARD_NAME}_${REV_UPPER}_system_wrapper\.pdi"
		ALLOWED_PATTERN+="|${BOARD_NAME}_es1_system\.xsa"
		ALLOWED_PATTERN+="|${BOARD_NAME}_${REV_UPPER}_es1_system\.xsa"
		ALLOWED_PATTERN+="|${BOARD_NAME}_system\.xsa"
		ALLOWED_PATTERN+="|${BOARD_NAME}_${REV_UPPER}_system\.xsa"
		ALLOWED_PATTERN+="|${BOARD_NAME}_strings\.js"
		ALLOWED_PATTERN+="|${BOARD_NAME}_versal_bit\.elf"
		ALLOWED_PATTERN+="|fancontrol_${BOARD_NAME}\.conf"
		ALLOWED_PATTERN+="|fancontrol_${BOARD_NAME}_${REV_LOWER}\.conf"
		ALLOWED_PATTERN+=")$"

		SOURCE_FILES=()
		TAR_FILE_NAMES=()
		SKIPPED_FILES=()

		for F in "${REPO_PATH}"/*; do
			if [ -f "$F" ]; then
				FNAME=$(basename "$F")
				if [[ "${FNAME}" =~ ${ALLOWED_PATTERN} ]]; then
					SOURCE_FILES+=("$F")
					TAR_FILE_NAMES+=("${FNAME}")
				else
					SKIPPED_FILES+=("${FNAME}")
				fi
			fi
		done

		if [ ${#SKIPPED_FILES[@]} -gt 0 ]; then
			echo ">>> Files not added (not in allowed list):"
			for SF in "${SKIPPED_FILES[@]}"; do
				echo "    - ${SF}"
			done
		fi

		if [ ${#SOURCE_FILES[@]} -eq 0 ]; then
			echo "Error: no allowed files found in '${REPO_PATH}'"
			exit 1
		fi
		;;
	*)
		echo "Error: unsupported target_file type '${TARGET_FILE}' (expected: json, elf, js, img, tar)"
		exit 1
		;;
esac

for SRC in "${SOURCE_FILES[@]}"; do
	if [ ! -f "${SRC}" ]; then
		echo "Error: source file '${SRC}' not found"
		exit 1
	fi
done

get_latest() {
	jf rt search --recursive=false --sort-by=name --sort-order=desc --include-dirs --limit=1 "${BASE_URL}/" | \
		python3 -c 'import sys, json; print(json.load(sys.stdin)[0]["path"])'
}

LATEST=$(get_latest)
LATEST="${LATEST%/}"
DATE_DIR="${RELEASE}_$(date +%Y%m%d%H%M)"
NEW_DIR="${BASE_URL}/${DATE_DIR}"

echo ">>> jf rt copy ${DRY_RUN} ${LATEST}(*) ${NEW_DIR}/{1}"
jf rt copy ${DRY_RUN} "${LATEST}(*)" "${NEW_DIR}/{1}"

if [ -n "${DRY_RUN}" ]; then
	echo ">>> [dry-run] Would download, update, and re-upload ${TAR_NAME}"
	exit 0
fi

TAR_PATH=$(jf rt search "${NEW_DIR}/**/${TAR_NAME}" | \
	python3 -c 'import sys, json; r=json.load(sys.stdin); print(r[0]["path"] if r else "")')

WORK_DIR=$(mktemp -d)
trap "rm -rf ${WORK_DIR}" EXIT

if [ -n "${TAR_PATH}" ]; then
	echo ">>> TAR_PATH: ${TAR_PATH}"

	echo ">>> Downloading ${TAR_PATH}"
	jf rt download --flat=true "${TAR_PATH}" "${WORK_DIR}/"

	if [ ! -f "${WORK_DIR}/${TAR_NAME}" ]; then
		echo "Error: download failed, ${WORK_DIR}/${TAR_NAME} not found"
		exit 1
	fi

	UNPACK_DIR="${WORK_DIR}/unpack"
	mkdir -p "${UNPACK_DIR}"
	tar xzf "${WORK_DIR}/${TAR_NAME}" -C "${UNPACK_DIR}"

	echo ">>> Contents of unpacked tar:"
	find "${UNPACK_DIR}" -type f

	for i in "${!SOURCE_FILES[@]}"; do
		SRC="${SOURCE_FILES[$i]}"
		TFN="${TAR_FILE_NAMES[$i]}"

		MATCH=$(find "${UNPACK_DIR}" -name "${TFN}" | head -1)
		if [ -z "${MATCH}" ]; then
			echo ">>> ${TFN} not found in ${TAR_NAME}, adding it"
			MATCH="${UNPACK_DIR}/${TFN}"
		else
			echo ">>> Found target: ${MATCH}"
			echo ">>> Old target checksum: $(md5sum "${MATCH}")"
		fi

		echo ">>> Source checksum: $(md5sum "${SRC}")"

		cp "${SRC}" "${MATCH}"

		echo ">>> New target checksum: $(md5sum "${MATCH}")"
	done

	echo ">>> Repackaging ${TAR_NAME}"
	(cd "${UNPACK_DIR}" && tar czf "${WORK_DIR}/${TAR_NAME}" *)

	echo ">>> Deleting old ${TAR_PATH}"
	jf rt delete --quiet "${TAR_PATH}"

	echo ">>> Uploading ${TAR_NAME} to ${TAR_PATH}"
	jf rt upload "${WORK_DIR}/${TAR_NAME}" "${TAR_PATH}"
else
	echo ">>> No existing tar found for ${BOARD_NAME}, creating new one"

	UNPACK_DIR="${WORK_DIR}/unpack"
	mkdir -p "${UNPACK_DIR}"

	for i in "${!SOURCE_FILES[@]}"; do
		cp "${SOURCE_FILES[$i]}" "${UNPACK_DIR}/${TAR_FILE_NAMES[$i]}"
	done

	echo ">>> Packaging ${TAR_NAME}"
	(cd "${UNPACK_DIR}" && tar czf "${WORK_DIR}/${TAR_NAME}" *)

	TAR_PATH="${NEW_DIR}/internal/${TAR_NAME}"
	echo ">>> Uploading new ${TAR_NAME} to ${TAR_PATH}"
	jf rt upload "${WORK_DIR}/${TAR_NAME}" "${TAR_PATH}"
fi
