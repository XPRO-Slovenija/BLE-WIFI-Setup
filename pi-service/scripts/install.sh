#!/usr/bin/env bash
#
# Build and install the Raspberry Pi BLE WiFi-provisioning service.
# Run on the Pi as root:  sudo ./scripts/install.sh
#
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root (use sudo)." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "==> Installing build & runtime dependencies"
apt-get update
apt-get install -y \
  build-essential cmake pkg-config \
  libglib2.0-dev \
  nlohmann-json3-dev \
  bluez

# Ensure at least one supported network backend + scan tool is present.
# (We do not force a particular stack; install whatever is missing.)
if ! command -v nmcli >/dev/null 2>&1 && ! command -v wpa_cli >/dev/null 2>&1; then
  echo "==> No network manager found; installing wpasupplicant + iw"
  apt-get install -y wpasupplicant iw
fi
command -v iw >/dev/null 2>&1 || apt-get install -y iw || true

echo "==> Configuring build"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" -j "$(nproc)"

echo "==> Installing binary to /usr/local/sbin"
install -m 0755 "$BUILD_DIR/rpi-ble-provision" /usr/local/sbin/rpi-ble-provision

echo "==> Installing systemd unit"
install -m 0644 "$PROJECT_DIR/systemd/rpi-ble-provision.service" \
  /etc/systemd/system/rpi-ble-provision.service

echo "==> Enabling and starting service"
systemctl daemon-reload
systemctl enable --now rpi-ble-provision.service

echo
echo "Done. Check status with:"
echo "  systemctl status rpi-ble-provision"
echo "  journalctl -u rpi-ble-provision -f"
