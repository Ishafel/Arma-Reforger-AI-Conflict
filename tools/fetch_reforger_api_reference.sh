#!/usr/bin/env bash
set -euo pipefail

readonly AICF_API_VERSION="1.8.0.10"
readonly AICF_API_COMMIT="b46bdd8f4932f3a256c765f93a44417996a6da73"
readonly AICF_API_SHA256="1ff7cfc1d13b23c64000afa5cbd5f2924c607218c0b093a27b4d7b0a31fb788a"
readonly AICF_API_URL="https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/archive/refs/tags/v${AICF_API_VERSION}.tar.gz"

AICF_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AICF_REPOSITORY_ROOT="$(cd "${AICF_SCRIPT_DIR}/.." && pwd)"
AICF_CACHE_ROOT="${AICF_REPOSITORY_ROOT}/.cache/reforger-api"
AICF_ARCHIVE="${AICF_CACHE_ROOT}/Arma-Reforger-Script-Diff-${AICF_API_VERSION}.tar.gz"
AICF_TARGET="${AICF_CACHE_ROOT}/Arma-Reforger-Script-Diff-${AICF_API_VERSION}"

if [[ -f "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c" ]] &&
	grep -q 'SCR_AIWorld.*spawn queue' "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c" &&
	grep -q 'void RequestSpawn' "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c"; then
	printf 'API reference is already available at %s\n' "${AICF_TARGET}"
	exit 0
fi

mkdir -p "${AICF_CACHE_ROOT}"

if [[ ! -f "${AICF_ARCHIVE}" ]]; then
	printf 'Downloading official Arma Reforger Script Diff %s...\n' "${AICF_API_VERSION}"
	curl --fail --location --output "${AICF_ARCHIVE}.part" "${AICF_API_URL}"
	AICF_DOWNLOADED_SHA256="$(shasum -a 256 "${AICF_ARCHIVE}.part" | awk '{print $1}')"
	if [[ "${AICF_DOWNLOADED_SHA256}" != "${AICF_API_SHA256}" ]]; then
		printf 'Checksum mismatch: expected %s, got %s. Partial archive left at %s.part\n' \
			"${AICF_API_SHA256}" "${AICF_DOWNLOADED_SHA256}" "${AICF_ARCHIVE}" >&2
		exit 1
	fi
	mv "${AICF_ARCHIVE}.part" "${AICF_ARCHIVE}"
fi

AICF_ARCHIVE_SHA256="$(shasum -a 256 "${AICF_ARCHIVE}" | awk '{print $1}')"
if [[ "${AICF_ARCHIVE_SHA256}" != "${AICF_API_SHA256}" ]]; then
	printf 'Cached archive checksum mismatch at %s. Remove that exact file and run again.\n' "${AICF_ARCHIVE}" >&2
	exit 1
fi

tar -xzf "${AICF_ARCHIVE}" -C "${AICF_CACHE_ROOT}"

if [[ ! -f "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c" ]] ||
	! grep -q 'SCR_AIWorld.*spawn queue' "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c" ||
	! grep -q 'void RequestSpawn' "${AICF_TARGET}/scripts/Game/Entities/SCR_AIGroup.c"; then
	printf 'Extracted snapshot does not contain the expected %s SCR_AIGroup queue API (commit %s).\n' \
		"${AICF_API_VERSION}" "${AICF_API_COMMIT}" >&2
	exit 1
fi

printf 'Verified API reference installed at %s\n' "${AICF_TARGET}"
