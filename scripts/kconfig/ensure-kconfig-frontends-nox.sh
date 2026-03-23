#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Allow user override to support offline/enterprise environments.
KCONFIG_FRONTENDS_DIR="${OPENEMBER_KCONFIG_FRONTENDS_DIR:-${ROOT_DIR}/.kconfig-frontends}"

if [[ -x "${KCONFIG_FRONTENDS_DIR}/usr/bin/kconfig-mconf" && -x "${KCONFIG_FRONTENDS_DIR}/usr/bin/kconfig-conf" ]]; then
  echo "${KCONFIG_FRONTENDS_DIR}"
  exit 0
fi

mkdir -p "${KCONFIG_FRONTENDS_DIR}"

DEB_URL="https://deb.debian.org/debian/pool/main/k/kconfig-frontends/kconfig-frontends-nox_4.11.0.1+dfsg-6_amd64.deb"

tmp_dir="$(mktemp -d)"
cleanup() { rm -rf "${tmp_dir}"; }
trap cleanup EXIT

deb_path="${tmp_dir}/kconfig-frontends-nox.deb"
echo "Downloading kconfig-frontends-nox..." >&2
curl -L --fail -o "${deb_path}" "${DEB_URL}"

echo "Extracting kconfig-frontends-nox to ${KCONFIG_FRONTENDS_DIR} ..." >&2
dpkg-deb -x "${deb_path}" "${KCONFIG_FRONTENDS_DIR}"

echo "${KCONFIG_FRONTENDS_DIR}"

