#!/bin/bash -e

# Install rpi-ble-provision (BLE WiFi provisioning service)
install -m 755 "files/install_rpi_ble_provision.sh" "${ROOTFS_DIR}/home/xpro/"
install -m 644 "files/rpi-ble-provision.service"    "${ROOTFS_DIR}/home/xpro/"

on_chroot << EOF
cd /home/xpro && ./install_rpi_ble_provision.sh
rm -f /home/xpro/install_rpi_ble_provision.sh /home/xpro/rpi-ble-provision.service
EOF
