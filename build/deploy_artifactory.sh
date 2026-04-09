#! /bin/bash

#
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

set -euo pipefail

trap 'echo "Error on line $LINENO (exit code $?)"' ERR

usage_classic() {
	echo "=== Classic deployment (-c / --classic) ==="
	echo ""
	echo "Usage: $0 -c -r <ver> [-d] <board_name> <target_file> <repo_path>"
	echo ""
	echo "  Flow: resolve sources from <repo_path> by <target_file> type, run get_latest on"
	echo "  Artifactory, copy the latest release folder to a new timestamped folder, then"
	echo "  update systemcontroller-app-<board>.tar.gz (create under .../internal/ if missing)."
	echo ""
	echo "  <target_file> must be one of: json | elf | js | img | tar"
	echo "    json  - board JSON (board/{BOARD}.json) and optional board/{BOARD}-XX.json"
	echo "    elf   - BIT/{BOARD}/versal_bit.elf → <board>_versal_bit.elf in tar"
	echo "    js    - src/static/js/<board>_strings.js"
	echo "    img   - images: {BOARD}_home.png, <board>.jpg"
	echo "    tar   - allowed loose files from a directory (<repo_path> = that directory)"
	echo ""
	echo "  Options: -c required; -r required; -d dry-run."
}

usage_new() {
	echo "=== New implementation (default, non-classic) ==="
	echo ""
	echo "Usage: $0 -r <release> [-b <internal|external>/<board_name>] [-d] [-f <artifactory-folder>] <target_file(s)>"
	echo ""
	echo "  -r, --release <release>  Required. Artifactory release folder (e.g. 2026.1 →"
	echo "                         system-controller/sc_app_bsp/<release>/)."
	echo ""
	echo "  -b, --board-info   internal/<board_name> or external/<board_name>. Work is limited to"
	echo "                     NEW_DIR/internal|external/systemcontroller-app-<board>.tar.gz only"
	echo "                     (no wildcard search across other tars)."
	echo "                     With -b, <target_file(s)> is one or more regular files."
	echo "                     If a file name lacks the board name (case-insensitive check), you are prompted once per"
	echo "                     such file before using <board_name>_<file> as the member name inside the tar."
	echo "                     Patch if that board tar exists, create if not (one or many member files)."
	echo ""
	echo "  <target_file(s)>  Path(s) from \$PWD (files only; directories are not accepted). Without -b:"
	echo "                    discover and patch. With -b: only that board tar."
	echo ""
	echo "  Without -f: get_latest → copy to a new timestamped folder, then patch or create under NEW_DIR."
	echo ""
	echo "  With -f <artifactory-folder>: path relative to system-controller/sc_app_bsp/<release>/ only"
	echo "  (not a full system-controller/... prefix). No copy step; patch the tar in that folder and re-upload."
	echo ""
	echo "  Options: -r|--release required; -b optional (single board tar only); -d dry-run; -f optional. Do not use -c."
}

usage() {
	usage_classic
	echo ""
	usage_new
	exit 1
}

# True if basename already contains board name as a substring (case-insensitive).
_nonclassic_basename_has_board() {
	local bn bd
	bn=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
	bd=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')
	[[ -n "${bd}" ]] && [[ "${bn}" == *"${bd}"* ]]
}

# With -b: if a member basename lacks the board name, use ${BOARD_NAME}_<basename> instead.
# Prompts once per file that needs a prefix (unless DRY_RUN: only prints planned prompts/names).
apply_board_prefix_tar_member_names() {
	local i
	local -a new_names=()
	local orig proposed _reply

	for i in "${!TAR_FILE_NAMES[@]}"; do
		orig="${TAR_FILE_NAMES[$i]}"
		if _nonclassic_basename_has_board "${orig}" "${BOARD_NAME}"; then
			new_names+=("${orig}")
			continue
		fi
		proposed="${BOARD_NAME}_${orig}"

		if [ -n "${DRY_RUN}" ]; then
			echo ">>> [dry-run] Would prompt per file: '${orig}' -> '${proposed}' (board '${BOARD_NAME}')" >&2
			new_names+=("${proposed}")
			continue
		fi

		echo "Member '${orig}' does not include board '${BOARD_NAME}'; proposed tar member name '${proposed}' (file on disk unchanged)." >&2
		_reply=""
		if [ -r /dev/tty ]; then
			read -r -p "Apply this rename? [y/N] " _reply </dev/tty
		else
			read -r -p "Apply this rename? [y/N] " _reply
		fi
		case "${_reply}" in
			y | Y | yes | YES)
				new_names+=("${proposed}")
				;;
			*)
				echo "Aborted." >&2
				exit 1
				;;
		esac
	done

	TAR_FILE_NAMES=("${new_names[@]}")
}

# Non-classic: resolve <target_file(s)> path relative to $PWD at invocation.
resolve_nonclassic_local_file() {
	local tf="$1"
	if [[ "${tf}" == /* ]]; then
		printf '%s\n' "${tf}"
		return 0
	fi
	local d
	d=$(dirname -- "${tf}")
	case "${d}" in
		"" | . | ./)
			printf '%s/%s\n' "${PWD}" "$(basename -- "${tf}")"
			;;
		*)
			printf '%s/%s\n' "${PWD}" "${tf}"
			;;
	esac
}

DRY_RUN=""
RELEASE=""
RELEASE_FOLDER=""
BOARD_INFO=""
BOARD_SCOPE=""
BOARD_NAME=""
CLASSIC=0
# Set to 1 after apply_board_prefix_tar_member_names runs in deploy() (before jf rt copy).
B_BOARD_PREFIX_DONE=""

if [ $# -eq 0 ]; then
	usage_classic
	echo ""
	usage_new
	exit 1
fi

if ! TEMP=$(getopt -o dcr:f:b: --long classic,dry-run,release:,release-folder:,board-info: -n "$(basename "$0")" -- "$@"); then
	usage
fi
eval set -- "${TEMP}"
while true; do
	case "$1" in
		-d | --dry-run)
			DRY_RUN="--dry-run"
			shift
			;;
		-c | --classic)
			CLASSIC=1
			shift
			;;
		-r | --release)
			RELEASE="$2"
			shift 2
			;;
		-f | --release-folder)
			RELEASE_FOLDER="$2"
			shift 2
			;;
		-b | --board-info)
			BOARD_INFO="$2"
			shift 2
			;;
		--)
			shift
			break
			;;
		*)
			usage
			;;
	esac
done

if [ -z "${RELEASE}" ]; then
	echo "Error: -r or --release is required (Artifactory release folder, e.g. 2026.1)" >&2
	usage
fi

if [ "${CLASSIC}" -eq 1 ]; then
	if [ -n "${BOARD_INFO}" ]; then
		echo "Error: -b/--board-info is not used with -c/--classic (board is the first positional argument)" >&2
		exit 1
	fi
	if [ $# -ne 3 ]; then
		usage
	fi
	BOARD_NAME="$1"
	TARGET_FILE="$2"
	REPO_PATH="$3"
else
	if [ -n "${BOARD_INFO}" ]; then
		case "${BOARD_INFO}" in
			internal/* | external/*) ;;
			*)
				echo "Error: -b/--board-info must start with 'internal/' or 'external/' (got: '${BOARD_INFO}')" >&2
				exit 1
				;;
		esac
		if [[ "${BOARD_INFO}" == internal/* ]]; then
			BOARD_SCOPE="internal"
			BOARD_NAME="${BOARD_INFO#internal/}"
		elif [[ "${BOARD_INFO}" == external/* ]]; then
			BOARD_SCOPE="external"
			BOARD_NAME="${BOARD_INFO#external/}"
		fi
		if [ -z "${BOARD_NAME}" ]; then
			echo "Error: -b/--board-info must include a board name after internal/ or external/" >&2
			exit 1
		fi
	fi
	if [ $# -lt 1 ]; then
		usage
	fi
	REPO_PATH=""
	NONCLASSIC_MODE=""
	NONCLASSIC_FILES=()
	TARGET_FILE=""
	MEMBER_NAME=""

	if [ $# -eq 1 ]; then
		TARGET_PATH=$(resolve_nonclassic_local_file "$1")
		if [ -f "${TARGET_PATH}" ]; then
			NONCLASSIC_MODE="file"
			TARGET_FILE="${TARGET_PATH}"
			MEMBER_NAME=$(basename -- "${TARGET_PATH}")
		elif [ -d "${TARGET_PATH}" ]; then
			echo "Error: <target_file(s)> must be file(s); directory '${TARGET_PATH}' is not supported. List files explicitly." >&2
			exit 1
		else
			echo "Error: <target_file(s)> must be an existing file: '${TARGET_PATH}'" >&2
			exit 1
		fi
	else
		NONCLASSIC_MODE="multi"
		for _arg in "$@"; do
			_rp=$(resolve_nonclassic_local_file "${_arg}")
			if [ -d "${_rp}" ]; then
				echo "Error: with multiple <target_file(s)>, each must be a regular file, not a directory: '${_arg}'" >&2
				exit 1
			fi
			if [ ! -f "${_rp}" ]; then
				echo "Error: not a regular file: '${_arg}' -> '${_rp}'" >&2
				exit 1
			fi
			NONCLASSIC_FILES+=("${_rp}")
		done
		mapfile -t NONCLASSIC_FILES < <(printf '%s\n' "${NONCLASSIC_FILES[@]}" | LC_ALL=C sort)
	fi
fi

if [ "${CLASSIC}" -eq 1 ] && [ -n "${RELEASE_FOLDER}" ]; then
	echo "Error: -f/--release-folder is not supported with -c/--classic" >&2
	echo "  Classic mode uses systemcontroller-app-<board>.tar.gz under a new copy of the latest release folder only." >&2
	exit 1
fi

# Latest child under BASE_URL: same as historic behavior (name sort, descending).
get_latest() {
	jf rt search --recursive=false --sort-by=name --sort-order=desc --include-dirs --limit=1 "${BASE_URL}/" | \
		python3 -c 'import sys, json; print(json.load(sys.stdin)[0]["path"])'
}

# True if local .tar.gz contains file members with all given basenames.
_tgz_contains_basenames() {
	local tgz="$1"
	shift
	python3 -c '
import tarfile, sys
tgz = sys.argv[1]
need = set(sys.argv[2:])
with tarfile.open(tgz, "r:gz") as t:
    have = set()
    for m in t.getmembers():
        if m.isfile():
            have.add(m.name.split("/")[-1])
sys.exit(0 if need <= have else 1)
' "$tgz" "$@"
}

# Without -b: list every systemcontroller-app-*.tar.gz under new_dir that contains all of the given
# member basenames (one path per line). For a single basename, every tar that contains that member matches.
discover_nonclassic_tar_paths() {
	local new_dir="$1"
	shift
	local -a names=("$@")
	local json paths path work tgz
	local -a matches=()

	json=$(jf rt search "${new_dir}/**/systemcontroller-app-*.tar.gz" 2>/dev/null || echo '[]')
	mapfile -t paths < <(python3 -c 'import sys, json; [print(x["path"]) for x in json.load(sys.stdin)]' <<<"${json}" 2>/dev/null || true)

	for path in "${paths[@]}"; do
		[ -z "${path}" ] && continue
		work=$(mktemp -d)
		if ! jf rt download --flat=true "${path}" "${work}/" >/dev/null; then
			rm -rf "${work}"
			continue
		fi
		tgz=$(basename "${path}")
		if [ ! -f "${work}/${tgz}" ]; then
			rm -rf "${work}"
			continue
		fi
		if _tgz_contains_basenames "${work}/${tgz}" "${names[@]}"; then
			matches+=("${path}")
		fi
		rm -rf "${work}"
	done

	local n=${#matches[@]}
	if [ "${n}" -eq 0 ]; then
		echo "Error: no systemcontroller-app-*.tar.gz under '${new_dir}' contains all of: ${names[*]}" >&2
		echo "  To upload a new board tar, use -b internal/<board> or -b external/<board> with <target_file(s)>." >&2
		exit 1
	fi
	printf '%s\n' "${matches[@]}" | sort -u
}

# Non-classic: with -b, only NEW_DIR/<scope>/systemcontroller-app-<board>.tar.gz (no discover); else discover by members.
nonclassic_deploy_to_board_tar() {
	if [ -n "${DRY_RUN}" ]; then
		if [ -n "${BOARD_NAME}" ]; then
			SOURCE_FILES=()
			TAR_FILE_NAMES=()
			if [ "${NONCLASSIC_MODE}" = file ]; then
				SOURCE_FILES=("${TARGET_FILE}")
				TAR_FILE_NAMES=("${MEMBER_NAME}")
			else
				local _df
				for _df in "${NONCLASSIC_FILES[@]}"; do
					SOURCE_FILES+=("${_df}")
					TAR_FILE_NAMES+=("$(basename -- "${_df}")")
				done
			fi
			apply_board_prefix_tar_member_names
			TAR_NAME="systemcontroller-app-${BOARD_NAME}.tar.gz"
			echo ">>> [dry-run] Would patch or create ${NEW_DIR}/${BOARD_SCOPE}/${TAR_NAME} using member name(s): ${TAR_FILE_NAMES[*]}"
		else
			if [ "${NONCLASSIC_MODE}" = file ]; then
				echo ">>> [dry-run] Would find every systemcontroller-app-*.tar.gz under ${NEW_DIR} containing ${MEMBER_NAME}, then patch each"
			else
				local _f
				for _f in "${NONCLASSIC_FILES[@]}"; do
					echo ">>> [dry-run] Would find every systemcontroller-app-*.tar.gz under ${NEW_DIR} containing $(basename -- "${_f}"), then patch each"
				done
			fi
		fi
		return 0
	fi

	if [ -n "${BOARD_NAME}" ]; then
		if [ -z "${B_BOARD_PREFIX_DONE}" ]; then
			SOURCE_FILES=()
			TAR_FILE_NAMES=()
			if [ "${NONCLASSIC_MODE}" = file ]; then
				SOURCE_FILES=("${TARGET_FILE}")
				TAR_FILE_NAMES=("${MEMBER_NAME}")
			else
				local f
				for f in "${NONCLASSIC_FILES[@]}"; do
					SOURCE_FILES+=("${f}")
					TAR_FILE_NAMES+=("$(basename -- "${f}")")
				done
			fi
			apply_board_prefix_tar_member_names
		fi
		TAR_NAME="systemcontroller-app-${BOARD_NAME}.tar.gz"
		TAR_PATH="${NEW_DIR}/${BOARD_SCOPE}/${TAR_NAME}"
		TAR_EXISTS=$(jf rt search --limit=1 "${TAR_PATH}" 2>/dev/null | \
			python3 -c 'import sys, json
try:
    r = json.load(sys.stdin)
    print(1 if r else 0)
except Exception:
    print(0)')

		if [ "${TAR_EXISTS}" -eq 1 ]; then
			echo ">>> Patching ${TAR_PATH} (-b: this board tar only)"
			tar_patch_and_upload
			return 0
		fi

		echo ">>> Creating new ${TAR_NAME} under ${NEW_DIR}/${BOARD_SCOPE}/"
		local NC_WORK
		NC_WORK=$(mktemp -d)
		trap "rm -rf ${NC_WORK}" EXIT
		local NC_UNPACK="${NC_WORK}/unpack"
		mkdir -p "${NC_UNPACK}"
		local i
		for i in "${!SOURCE_FILES[@]}"; do
			cp "${SOURCE_FILES[$i]}" "${NC_UNPACK}/${TAR_FILE_NAMES[$i]}"
		done
		(cd "${NC_UNPACK}" && tar czf "${NC_WORK}/${TAR_NAME}" *)
		TAR_PATH="${NEW_DIR}/${BOARD_SCOPE}/${TAR_NAME}"
		echo ">>> Uploading new ${TAR_NAME} to ${TAR_PATH}"
		jf rt upload "${NC_WORK}/${TAR_NAME}" "${TAR_PATH}"
		return 0
	fi

	if [ "${NONCLASSIC_MODE}" = file ]; then
		SOURCE_FILES=("${TARGET_FILE}")
		TAR_FILE_NAMES=("${MEMBER_NAME}")
		local -a DISCOVERED_TARS=()
		mapfile -t DISCOVERED_TARS < <(discover_nonclassic_tar_paths "${NEW_DIR}" "${TAR_FILE_NAMES[@]}")
		local tp
		for tp in "${DISCOVERED_TARS[@]}"; do
			TAR_PATH="${tp}"
			TAR_NAME=$(basename "${TAR_PATH}")
			echo ">>> Patching ${TAR_PATH}"
			tar_patch_and_upload
		done
		return 0
	fi

	local df
	for df in "${NONCLASSIC_FILES[@]}"; do
		SOURCE_FILES=("${df}")
		TAR_FILE_NAMES=("$(basename -- "${df}")")
		local -a DISCOVERED_TARS=()
		mapfile -t DISCOVERED_TARS < <(discover_nonclassic_tar_paths "${NEW_DIR}" "${TAR_FILE_NAMES[@]}")
		local tp
		for tp in "${DISCOVERED_TARS[@]}"; do
			TAR_PATH="${tp}"
			TAR_NAME=$(basename "${TAR_PATH}")
			echo ">>> Patching ${TAR_PATH} (member ${TAR_FILE_NAMES[0]})"
			tar_patch_and_upload
		done
	done
}

# Download tar at TAR_PATH, replace members from SOURCE_FILES/TAR_FILE_NAMES, re-upload to TAR_PATH.
tar_patch_and_upload() {
	WORK_DIR=$(mktemp -d)
	trap "rm -rf ${WORK_DIR}" EXIT

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
	find "${UNPACK_DIR}" -type f || true

	for i in "${!SOURCE_FILES[@]}"; do
		SRC="${SOURCE_FILES[$i]}"
		TFN="${TAR_FILE_NAMES[$i]}"

		MATCH=""
		while IFS= read -r _m; do
			MATCH="${_m}"
			break
		done < <(find "${UNPACK_DIR}" -name "${TFN}" 2>/dev/null)
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
	set +e
	jf rt upload "${WORK_DIR}/${TAR_NAME}" "${TAR_PATH}"
	_jf_upload_st=$?
	set -e
	if [ "${_jf_upload_st}" -ne 0 ]; then
		echo ">>> Warning: jf rt upload exited with status ${_jf_upload_st} (output above may still show success). Verify the artifact in Artifactory." >&2
	fi
	rm -rf "${WORK_DIR}"
	trap - EXIT 2>/dev/null || true
}

deploy() {
BASE_URL="system-controller/sc_app_bsp/$RELEASE"

if [ -n "${RELEASE_FOLDER}" ]; then
	RF="${RELEASE_FOLDER#/}"
	RF="${RF%/}"
	if [[ "${RF}" == system-controller/sc_app_bsp/* ]]; then
		echo "Error: -f/--release-folder must be relative to system-controller/sc_app_bsp/<release>/ (from -r), not a path starting with system-controller/sc_app_bsp/ (got: '${RELEASE_FOLDER}')" >&2
		echo "  Example: -r ${RELEASE} -f my_folder  →  ${BASE_URL}/my_folder" >&2
		exit 1
	fi
	NEW_DIR="${BASE_URL}/${RF}"
	FOLDER_CHECK=$(jf rt search --recursive=false --include-dirs --limit=1 "${NEW_DIR}/" | \
		python3 -c 'import sys, json; r=json.load(sys.stdin); print(len(r))')
	if [ "${FOLDER_CHECK}" -eq 0 ]; then
		echo "Error: --release-folder '${NEW_DIR}' does not exist in Artifactory"
		exit 1
	fi
	echo ">>> Using existing release folder (no copy): ${NEW_DIR}"
else
	RELEASE_CHECK=$(jf rt search --recursive=false --include-dirs --limit=1 "${BASE_URL}/" | \
		python3 -c 'import sys, json; r=json.load(sys.stdin); print(len(r))')
	if [ "${RELEASE_CHECK}" -eq 0 ]; then
		echo "Error: release folder '${BASE_URL}' does not exist in Artifactory"
		exit 1
	fi
fi

# Non-classic only: -f → patch via discover, or with -b only the named board tar under NEW_DIR/internal|external/.
if [ "${CLASSIC}" -eq 0 ] && [ -n "${RELEASE_FOLDER}" ]; then
	nonclassic_deploy_to_board_tar
	[ -n "${DRY_RUN}" ] && exit 0
	return
fi

if [ "${CLASSIC}" -eq 1 ]; then
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

	LATEST=$(get_latest)
	echo ">>> Latest folder under release (name sort desc): ${LATEST}"
	LATEST="${LATEST%/}"
	DATE_DIR="${RELEASE}_$(date +%Y%m%d%H%M)"
	NEW_DIR="${BASE_URL}/${DATE_DIR}"
	echo ">>> New duplicate folder: ${NEW_DIR}"

	echo ">>> jf rt copy ${DRY_RUN} ${LATEST}(*) ${NEW_DIR}/{1}"
	jf rt copy ${DRY_RUN} "${LATEST}(*)" "${NEW_DIR}/{1}"

	if [ -n "${DRY_RUN}" ]; then
		echo ">>> [dry-run] Would download, update, and re-upload ${TAR_NAME}"
		exit 0
	fi

	TAR_PATH=$(jf rt search "${NEW_DIR}/**/${TAR_NAME}" | \
		python3 -c 'import sys, json; r=json.load(sys.stdin); print(r[0]["path"] if r else "")')

	if [ -n "${TAR_PATH}" ]; then
		tar_patch_and_upload
	else
		WORK_DIR=$(mktemp -d)
		trap "rm -rf ${WORK_DIR}" EXIT

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
	return
fi

# Non-classic, no -f: same copy as classic, then find tar by member basename under NEW_DIR.
# With -b, interactive member renames must run before jf rt copy (otherwise abort still leaves a new folder).
if [ "${CLASSIC}" -eq 0 ] && [ -z "${RELEASE_FOLDER}" ] && [ -n "${BOARD_NAME}" ] && [ -z "${DRY_RUN}" ]; then
	SOURCE_FILES=()
	TAR_FILE_NAMES=()
	if [ "${NONCLASSIC_MODE}" = file ]; then
		SOURCE_FILES=("${TARGET_FILE}")
		TAR_FILE_NAMES=("${MEMBER_NAME}")
	else
		for f in "${NONCLASSIC_FILES[@]}"; do
			SOURCE_FILES+=("${f}")
			TAR_FILE_NAMES+=("$(basename -- "${f}")")
		done
	fi
	apply_board_prefix_tar_member_names
	B_BOARD_PREFIX_DONE=1
fi

LATEST=$(get_latest)
echo ">>> Latest folder under release (name sort desc): ${LATEST}"
LATEST="${LATEST%/}"
DATE_DIR="${RELEASE}_$(date +%Y%m%d%H%M)"
NEW_DIR="${BASE_URL}/${DATE_DIR}"
echo ">>> New duplicate folder: ${NEW_DIR}"

echo ">>> jf rt copy ${DRY_RUN} ${LATEST}(*) ${NEW_DIR}/{1}"
jf rt copy ${DRY_RUN} "${LATEST}(*)" "${NEW_DIR}/{1}"

if [ -n "${DRY_RUN}" ]; then
	nonclassic_deploy_to_board_tar
	exit 0
fi

nonclassic_deploy_to_board_tar
}

deploy
