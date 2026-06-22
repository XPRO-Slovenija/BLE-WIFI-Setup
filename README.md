# Pi Provisioner

Set up a headless Raspberry Pi's WiFi from your phone over Bluetooth Low Energy —
no keyboard, screen, or existing network required.

Pi Provisioner is a two-part system:

| Component | Path | What it is |
|---|---|---|
| **Mobile app** | [`mobile/`](mobile/) | Flutter (iOS / Android) client that finds a Pi over BLE and sends it WiFi credentials. |
| **Pi service** | [`pi-service/`](pi-service/) | C++ BLE peripheral (`rpi-ble-provision`) that runs on the Pi, advertises a GATT service, and applies the credentials. |

Both sides implement the same GATT contract, so they can be developed and
shipped independently.

## How it works

1. **Advertise** — the Pi service advertises a provisioning GATT service (shown
   as `rpi-prov-*`).
2. **Scan & connect** — the app finds the Pi and connects.
3. **Bond** — the credential and status characteristics require BLE pairing
   (Just Works), so a bonded, link-layer-encrypted connection is established
   before any secrets move.
4. **Pick a network** — the app reads the SSID list the Pi can see (or you type
   one), and you enter the password.
5. **Provision** — the app writes the credentials; the Pi joins the network and
   streams live status (`connecting` → `connected` with an IP, or `failed`).

## GATT contract

- Service `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00`
- SSID list `…4e01` — `encrypt-read` / `encrypt-notify`, JSON `[{ssid,signal,secured}]`
- Credentials `…4e02` — `encrypt-write`, JSON `{ssid,psk}`
- Status `…4e03` — `encrypt-read` / `encrypt-notify`, JSON `{state,ip,ssid,error}`

The `encrypt-*` flags force BlueZ to require a bond before a characteristic can
be accessed — link-layer encryption over the bonded connection is the security
model. See [`pi-service/README.md`](pi-service/README.md) for the full
specification and security notes.

## Quick start

**Pi side** (on the Raspberry Pi):

```sh
cd pi-service
sudo ./scripts/install.sh
```

**App side** (on a real phone — BLE does not work on simulators):

```sh
cd mobile
flutter pub get
flutter run
```

See each component's README for full build, run, and verification instructions:
[`mobile/README.md`](mobile/README.md) · [`pi-service/README.md`](pi-service/README.md).

## Documentation

- [Privacy](privacy.md) — what data the app handles and how.
- [Copyright](copyright.md) — ownership and third-party licenses.
- [Marketing](marketing.md) — product overview.

## License & copyright

© 2026 XPRO Slovenija. All rights reserved. See [copyright.md](copyright.md).

> **Note on `flutter_blue_plus`:** the app currently uses `License.nonprofit`,
> which is free for nonprofit/personal use only. Commercial distribution
> requires a paid plugin license or a different BLE plugin. See
> [copyright.md](copyright.md).
