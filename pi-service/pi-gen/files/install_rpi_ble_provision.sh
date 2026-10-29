#!/usr/bin/env bash
#
# Install the rpi-ble-provision BLE WiFi-provisioning service from a GitHub
# release. Runs inside the pi-gen chroot (or on a live Pi as root).
#
# Expects rpi-ble-provision.service to sit next to this script.
#
set -euo pipefail

VERSION="${RPI_BLE_PROVISION_VERSION:-v1.0.0}"
REPO="XPRO-Slovenija/BLE-WIFI-Setup"
URL="${RPI_BLE_PROVISION_URL:-https://github.com/${REPO}/releases/download/${VERSION}/rpi-ble-provision}"
# Optional: export RPI_BLE_PROVISION_SHA256=<sum> to verify the download.
SHA256="${RPI_BLE_PROVISION_SHA256:-}"

BIN_DEST=/usr/local/sbin/rpi-ble-provision
UNIT_DEST=/etc/systemd/system/rpi-ble-provision.service
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root (use sudo)." >&2
  exit 1
fi

echo "==> Installing runtime dependencies"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  bluez \
  libglib2.0-0 \
  ca-certificates \
  curl

# Ensure at least one supported network backend + scan tool is present.
if ! command -v nmcli >/dev/null 2>&1 && ! command -v wpa_cli >/dev/null 2>&1; then
  echo "==> No network manager found; installing wpasupplicant"
  apt-get install -y --no-install-recommends wpasupplicant
fi
command -v iw >/dev/null 2>&1 || apt-get install -y --no-install-recommends iw || true

echo "==> Downloading ${URL}"
TMP_BIN="$(mktemp)"
trap 'rm -f "$TMP_BIN"' EXIT
curl -fsSL --retry 3 --retry-delay 2 -o "$TMP_BIN" "$URL"

if [[ -n "$SHA256" ]]; then
  echo "==> Verifying checksum"
  echo "${SHA256}  ${TMP_BIN}" | sha256sum -c -
fi

# Sanity check: a 404/HTML error page would otherwise be installed as "the binary".
if ! head -c 4 "$TMP_BIN" | grep -q $'\x7fELF'; then
  echo "Downloaded file is not an ELF binary — check ${URL}" >&2
  exit 1
fi

echo "==> Installing binary to ${BIN_DEST}"
install -m 0755 "$TMP_BIN" "$BIN_DEST"

echo "==> Installing systemd unit"
install -m 0644 "${SCRIPT_DIR}/rpi-ble-provision.service" "$UNIT_DEST"

echo "==> Enabling service"
# systemctl works in the pi-gen chroot for enable/disable, but fall back to a
# manual wants-symlink in case the chroot has no working systemd binary.
if systemctl enable rpi-ble-provision.service 2>/dev/null; then
  :
else
  mkdir -p /etc/systemd/system/multi-user.target.wants
  ln -sf "$UNIT_DEST" \
    /etc/systemd/system/multi-user.target.wants/rpi-ble-provision.service
fi

# On a running system, start it now; in the chroot this is a no-op.
if [[ -d /run/systemd/system ]]; then
  systemctl daemon-reload
  systemctl restart rpi-ble-provision.service
fi

echo "==> rpi-ble-provision ${VERSION} installed"
