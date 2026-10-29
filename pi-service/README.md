# rpi-ble-provision — Raspberry Pi BLE WiFi provisioning service

A C++ BLE peripheral for headless Raspberry Pis. It advertises itself over
Bluetooth Low Energy and exposes a GATT service that a phone connects to in
order to set the Pi's WiFi credentials — useful when the Pi has no keyboard,
screen, or existing network.

- Talks to **BlueZ** over the system **D-Bus** (GLib/GDBus + `GMainLoop`).
- Requires **BLE pairing/bonding** before credentials can be written
  (the credentials characteristic is flagged `encrypt-write`).
- Applies WiFi by **detecting the active network stack at runtime** and using
  either **NetworkManager (`nmcli`)** or **wpa_supplicant (`wpa_cli`/`iw`)**.

> Scope: this is the **Raspberry Pi side**. The companion Flutter app (iOS /
> Android) is a separate, later deliverable and must implement the GATT contract
> below.

## BLE GATT contract

Advertised as `rpi-prov-<suffix>` (suffix = last 4 of the Pi serial, else
hostname) unless a name is configured, including the service UUID so the app
can filter scans. Both are configurable (see [Configuration](#configuration));
the table shows the defaults.

| Role | UUID | Properties | Payload |
|---|---|---|---|
| Provisioning **Service** | `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00` | primary | — |
| **SSID list** char | `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e01` | `encrypt-read`, `encrypt-notify` | read: JSON array of networks |
| **Credentials** char | `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e02` | `encrypt-write` | write: JSON credentials |
| **Status** char | `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e03` | `encrypt-read`, `encrypt-notify` | read/notify: JSON status |

The `encrypt-*` flags force BlueZ to require a bond (Just Works pairing via a
`NoInputNoOutput` agent) before the characteristic can be accessed — this is the
chosen security model: link-layer encryption over the bonded connection.

**SSID list** (read):
```json
[{"ssid":"HomeWiFi","signal":82,"secured":true}, ...]
```

**Credentials** (write — open networks omit `psk`):
```json
{"ssid":"HomeWiFi","psk":"s3cret"}
```

**Status** (read / notify):
```json
{"state":"idle|connecting|connected|failed","ip":"192.168.1.42","ssid":"HomeWiFi","error":""}
```

Typical app flow: scan → connect → bond → read SSID list → subscribe to Status →
write Credentials → watch Status go `connecting` → `connected` (with `ip`).

## Build & install (on the Pi)

```sh
sudo ./scripts/install.sh
```

This installs dependencies (`build-essential cmake pkg-config libglib2.0-dev
nlohmann-json3-dev bluez`, plus `wpasupplicant`/`iw` if no network manager is
present), builds with CMake, installs the binary to `/usr/local/sbin`, and
enables the `systemd` unit.

Manual build:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo ./build/rpi-ble-provision   # run in foreground for debugging
```

## Configuration

Settings come from `/etc/rpi-ble-provision.conf` (optional; installed with
everything commented out) and are overridden by command-line flags:

| Flag | Config key | Meaning |
|---|---|---|
| `-c, --config PATH` | — | Read this config file instead (must exist) |
| `-u, --service-uuid UUID` | `service_uuid` | Service UUID; **must end in `00`**. Characteristics are the same UUID ending `01` (SSID list), `02` (credentials), `03` (status) |
| `-n, --name NAME` | `name` | Advertised Bluetooth name |
| `-N, --name-file PATH` | `name_file` | Read the name from the first line of a file |

Use `name` or `name_file`, not both. A configured name also becomes the
adapter alias, so phones show it after connecting too. Names over 29 bytes are
accepted with a warning, but BlueZ truncates them in the advertisement.

```ini
# /etc/rpi-ble-provision.conf
service_uuid = 11111111-2222-3333-4444-555555555500
name = Kitchen Display
```

Settings are read only at startup. `rpi-ble-provision-reload.path` (enabled by
`install.sh`) restarts the service whenever `/etc/rpi-ble-provision.conf` or
`/etc/rpi-ble-provision.name` changes, so use that path for `name_file` to get
automatic renames. To watch a different file, add a `PathChanged=` line with
`systemctl edit rpi-ble-provision-reload.path`. The restart drops any phone
that is connected at the time.

Invalid configuration exits with status 2, and the systemd unit does not restart
on that status. The mobile app scans for the default service UUID, so a custom
UUID needs a matching app build.

Service management:
```sh
systemctl status rpi-ble-provision
journalctl -u rpi-ble-provision -f
```

## Architecture

```
src/
  main.cpp              GLib main loop, signal handling, wiring
  dbus/bluez.*          BlueZ D-Bus helpers (connect, find adapter, set prop, call)
  gatt/server.*         GATT app: ObjectManager, service/characteristics,
                        advertisement, pairing agent, credential->connect flow
  wifi/backend.h        Backend interface (scan / connect / status)
  wifi/networkmanager.* nmcli backend
  wifi/wpa_supplicant.* wpa_cli + iw backend
  wifi/detect.*         runtime backend selection
  util/subprocess.*     safe fork/exec argv runner (no shell)
  util/log.h            stderr/journal logging
```

The slow WiFi join runs on a detached worker thread; its result is marshalled
back to the GLib main loop via `g_idle_add` before emitting a Status
notification.

## Security notes

- **No shell.** All external tools (`nmcli`, `wpa_cli`, `iw`, `systemctl`) are
  invoked via `fork`/`exec` with an argv vector (`util::subprocess`). User-
  supplied SSID/PSK are never passed through a shell, so shell metacharacters
  cannot be interpreted as commands.
- **Control-char rejection.** SSID/PSK containing control characters (newlines,
  NUL, …) are rejected before reaching `wpa_cli`, preventing config injection
  into wpa_supplicant's line-based control interface.
- **Bonding required.** Credentials can only be written over a bonded
  (encrypted) link.
- Runs as **root** for now (BlueZ system-bus access + `nmcli`/`wpa_cli`).
- Future hardening (not in this pass): numeric-comparison pairing
  (`DisplayYesNo`) for MITM resistance, app-level payload encryption, and a
  capability-scoped non-root user with a tailored D-Bus policy.

## Verification (manual, on real hardware)

BlueZ is Linux-only, so testing happens on the Pi with a phone (e.g. **nRF
Connect**):

1. **Advertising** — `sudo systemctl start rpi-ble-provision`; in `bluetoothctl`
   confirm the adapter advertises `rpi-prov-…` + the service UUID; the journal
   shows `provisioning service ready`.
2. **Discover + bond** — from nRF Connect, find the device, connect, and confirm
   a Just Works bond is established.
3. **Read SSIDs** — read the SSID-list characteristic; JSON should match
   `nmcli device wifi list` / `wpa_cli scan_results`.
4. **Provision** — subscribe to Status, write
   `{"ssid":"<test>","psk":"<pw>"}` to Credentials, and watch Status go
   `connecting` → `connected` with an IP. Verify on the Pi with `iwgetid` /
   `nmcli device status`.
5. **Both stacks** — repeat step 4 on a NetworkManager (Bookworm) image and with
   `wpa_supplicant` active to exercise detection + both backends.
6. **Injection check** — write an SSID like `a; reboot` and confirm it is treated
   as a literal SSID (no command runs, the Pi does not reboot).
