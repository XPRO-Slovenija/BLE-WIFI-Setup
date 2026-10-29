# pi-gen stage files

Drop-in files for a pi-gen custom stage that installs the prebuilt
`rpi-ble-provision` binary from a GitHub release (no compiling in the chroot).

Copy into your stage directory so it looks like:

```
stage-xpro/
  00-run.sh                                  <- from here
  files/
    install_rpi_ble_provision.sh             <- from here
    rpi-ble-provision.service                <- from here
```

`00-run.sh` copies both files into `${ROOTFS_DIR}/home/xpro/` and runs the
installer via `on_chroot`; the installer pulls the release binary, installs it
to `/usr/local/sbin`, drops the unit in `/etc/systemd/system`, and enables it
for `multi-user.target`. The staged files are removed afterwards.

## Version / URL

Defaults to `v1.0.0`. Override per build by exporting into the chroot, e.g.:

```sh
on_chroot << EOF
cd /home/xpro && RPI_BLE_PROVISION_VERSION=v1.1.0 ./install_rpi_ble_provision.sh
EOF
```

Other env vars: `RPI_BLE_PROVISION_URL` (full override),
`RPI_BLE_PROVISION_SHA256` (verify the download — recommended for reproducible
images).

## Notes

- The chroot needs working network + DNS for the download; pi-gen normally has
  this since it runs `apt-get` in the same chroot.
- The release binary must match the image architecture (arm64 vs armhf). The
  installer rejects non-ELF downloads (e.g. a 404 page) but does not check the
  ELF machine type.
- The service is enabled but not started at build time; it comes up on first
  boot.
