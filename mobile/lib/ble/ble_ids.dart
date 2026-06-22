import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// GATT contract shared with the Raspberry Pi service (see pi-service/README.md).
/// These UUIDs MUST stay in sync with src/gatt/server.h on the device.
class BleIds {
  BleIds._();

  /// Provisioning service. The Pi advertises this UUID, so we scan for it.
  static final Guid service = Guid('7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00');

  /// Read/notify: JSON array of visible networks `[{ssid,signal,secured}]`.
  static final Guid ssidList = Guid('7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e01');

  /// Write: JSON credentials `{"ssid":"...","psk":"..."}` (encrypt-write).
  static final Guid credentials = Guid('7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e02');

  /// Read/notify: JSON status `{state,ip,ssid,error}`.
  static final Guid status = Guid('7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e03');

  /// Devices advertise as `rpi-prov-<suffix>`.
  static const String namePrefix = 'rpi-prov-';
}
