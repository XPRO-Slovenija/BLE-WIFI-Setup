import 'dart:io' show Platform;

import 'package:permission_handler/permission_handler.dart';

/// Request the runtime permissions BLE scanning needs.
///
/// Android 12+: BLUETOOTH_SCAN + BLUETOOTH_CONNECT. Android 11 and below also
/// need location for scanning. iOS surfaces the Bluetooth prompt automatically
/// on first use, so there is nothing to request here.
Future<bool> requestBlePermissions() async {
  if (!Platform.isAndroid) return true;

  final statuses = await [
    Permission.bluetoothScan,
    Permission.bluetoothConnect,
    Permission.locationWhenInUse,
  ].request();

  // Scan + connect are the ones we strictly require on modern Android; location
  // only matters on API <= 30, where the above still resolves it.
  final scanOk = statuses[Permission.bluetoothScan]?.isGranted ?? false;
  final connectOk = statuses[Permission.bluetoothConnect]?.isGranted ?? false;
  return scanOk && connectOk;
}
