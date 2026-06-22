# rpi_provisioner — Flutter app for Pi WiFi provisioning

iOS/Android app that finds a Raspberry Pi over Bluetooth LE and sets its WiFi.
It is the client for the `pi-service/` BLE peripheral and implements the same
GATT contract.

## Flow

1. **Scan** — scans for peripherals advertising the provisioning service UUID
   (shown as `rpi-prov-*`).
2. **Connect & pair** — connects and bonds. The SSID-list / credentials /
   status characteristics are `encrypt-*`, so the OS pairing dialog appears
   (Just Works). On Android we call `createBond()`; on iOS pairing is triggered
   automatically by the first encrypted read.
3. **Pick network** — reads the SSID list the Pi can see (or enter one manually),
   type the password.
4. **Send & watch** — writes the credentials, then shows live status from the
   Pi's status characteristic: `connecting` → `connected` (with IP) or `failed`.

## Project layout

```
lib/
  main.dart                       app entry, theme
  ble/ble_ids.dart                GATT UUIDs (must match pi-service/src/gatt/server.h)
  ble/ble_permissions.dart        runtime permission requests (Android)
  ble/provisioning_service.dart   connect/bond/discover, read SSIDs, write creds, status stream
  models/wifi_network.dart        SSID-list item
  models/provisioning_status.dart status payload + state enum
  screens/scan_screen.dart        device discovery list
  screens/provision_screen.dart   SSID picker, password, send, live status
```

## GATT contract

Mirrors `pi-service/README.md`:

- Service `7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00`
- SSID list `…4e01` — read/notify, JSON `[{ssid,signal,secured}]`
- Credentials `…4e02` — encrypt-write, JSON `{ssid,psk}`
- Status `…4e03` — read/notify, JSON `{state,ip,ssid,error}`

## Permissions

- **Android** (`android/app/src/main/AndroidManifest.xml`): `BLUETOOTH_SCAN`
  (with `neverForLocation`), `BLUETOOTH_CONNECT`; legacy `BLUETOOTH`/
  `BLUETOOTH_ADMIN`/`ACCESS_FINE_LOCATION` capped at API 30.
- **iOS** (`ios/Runner/Info.plist`): `NSBluetoothAlwaysUsageDescription`.

## ⚠️ flutter_blue_plus licensing

This uses `flutter_blue_plus` v2, whose `connect()` requires a `License`. We pass
`License.nonprofit`, which is **free for nonprofit/personal use only**.
**Commercial distribution requires a paid license** from the plugin author. If
this ships as a product, either purchase the license or switch BLE plugins
before release.

## Build & run

```sh
flutter pub get
flutter run                       # on a real device (BLE needs hardware)
flutter analyze
flutter test
flutter build apk --debug         # Android
flutter build ios --no-codesign   # iOS (use Xcode for signed builds)
```

BLE does not work on simulators/emulators — test on a physical phone with a Pi
running `rpi-ble-provision` nearby.
