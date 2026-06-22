# Copyright

**Pi Provisioner**

© 2026 XPRO Slovenija. All rights reserved.

This software and its source code, including the `mobile/` Flutter application
and the `pi-service/` Raspberry Pi service, are the property of XPRO Slovenija.
Unless a separate written license is provided, no part of this work may be
copied, modified, distributed, or used to create derivative works without prior
written permission from the copyright holder.

Contact: developer@xpro.si

## Trademarks

"Raspberry Pi" is a trademark of Raspberry Pi Ltd. "Bluetooth" is a trademark of
Bluetooth SIG, Inc. "Android" is a trademark of Google LLC. "iOS" is a trademark
of Apple Inc. Other product and company names mentioned herein may be the
trademarks of their respective owners. Their use does not imply any affiliation
with or endorsement by them.

## Third-party software

This project builds on third-party components, each governed by its own license.
The list below is informative; consult each project for authoritative terms.

### Mobile app (`mobile/`)

| Component | Use | License |
|---|---|---|
| Flutter / Dart SDK | Application framework | BSD-3-Clause |
| `permission_handler` | Runtime permissions | MIT |
| `flutter_secure_storage` | Local credential storage | BSD-3-Clause |
| `flutter_blue_plus` | Bluetooth LE | See note below |

> ⚠️ **`flutter_blue_plus` licensing.** The app calls `connect()` with
> `License.nonprofit`, which is **free for nonprofit and personal use only**.
> **Commercial distribution requires a paid license** from the plugin author. If
> Pi Provisioner ships as a commercial product, either purchase the appropriate
> license or migrate to a differently-licensed BLE plugin before release.

### Pi service (`pi-service/`)

| Component | Use | License |
|---|---|---|
| GLib / GDBus | D-Bus + main loop | LGPL-2.1+ |
| BlueZ | Bluetooth stack | GPL-2.0+ (daemon) / LGPL-2.1+ (libs) |
| `nlohmann/json` | JSON parsing | MIT |
| NetworkManager (`nmcli`) | WiFi backend (runtime) | GPL-2.0+ |
| wpa_supplicant (`wpa_cli`) / `iw` | WiFi backend (runtime) | BSD / GPL-2.0 |

The Pi service invokes NetworkManager and wpa_supplicant tooling as external
processes at runtime; it does not link against or redistribute them.
