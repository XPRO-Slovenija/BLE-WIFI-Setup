import 'dart:async';
import 'dart:convert';
import 'dart:io' show Platform;

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/provisioning_status.dart';
import '../models/wifi_network.dart';
import 'ble_ids.dart';

/// Wraps a single connection to a Raspberry Pi provisioning peripheral:
/// connect + bond, discover the GATT characteristics, read the SSID list,
/// stream status, and write credentials.
class ProvisioningService {
  ProvisioningService(this.device);

  final BluetoothDevice device;

  BluetoothCharacteristic? _ssidChar;
  BluetoothCharacteristic? _credsChar;
  BluetoothCharacteristic? _statusChar;

  /// Connect, bond (Android), and locate the provisioning characteristics.
  Future<void> connectAndDiscover() async {
    if (!device.isConnected) {
      // License.nonprofit is flutter_blue_plus's free tier; commercial use of
      // the plugin requires a paid license (see pubspec note / README).
      await device.connect(
        license: License.nonprofit,
        timeout: const Duration(seconds: 20),
      );
    }

    // On Android, bond explicitly so the encrypt-* characteristics become
    // accessible. iOS pairs automatically on first encrypted access (the
    // SSID-list read below), so no explicit step is needed there.
    if (Platform.isAndroid) {
      try {
        final bonded = await FlutterBluePlus.bondedDevices;
        if (!bonded.any((d) => d.remoteId == device.remoteId)) {
          await device.createBond();
        }
      } catch (_) {
        // Already bonded or the platform will pair on access — ignore.
      }
    }

    final services = await device.discoverServices();
    final svc = services.firstWhere(
      (s) => s.serviceUuid == BleIds.service,
      orElse: () => throw const ProvisioningException(
          'This device is not a Pi provisioning service.'),
    );

    for (final c in svc.characteristics) {
      final uuid = c.characteristicUuid;
      if (uuid == BleIds.ssidList) {
        _ssidChar = c;
      } else if (uuid == BleIds.credentials) {
        _credsChar = c;
      } else if (uuid == BleIds.status) {
        _statusChar = c;
      }
    }

    if (_ssidChar == null || _credsChar == null || _statusChar == null) {
      throw const ProvisioningException('Provisioning characteristics missing.');
    }
  }

  /// The characteristics are encrypt-*, so the first access on a fresh
  /// connection triggers pairing and can fail with "insufficient
  /// encryption/authentication" while the bond is being established. Retry a few
  /// times so the call succeeds once the link is encrypted.
  // Total window must outlast a user tapping the iOS pairing dialog (which can
  // take several seconds), not just a sub-second encryption race.
  Future<T> _withEncryptionRetry<T>(Future<T> Function() op,
      {int attempts = 20}) async {
    for (var i = 0;; i++) {
      try {
        return await op();
      } on FlutterBluePlusException catch (e) {
        final desc = (e.description ?? '').toLowerCase();
        final encryptionIssue = e.code == 15 || // insufficient encryption
            e.code == 5 || // insufficient authentication
            desc.contains('encryption') ||
            desc.contains('authentication');
        if (!encryptionIssue || i >= attempts - 1) rethrow;
        await Future.delayed(const Duration(seconds: 1));
      }
    }
  }

  /// Collect the list of networks the Pi can see.
  ///
  /// The full list exceeds one BLE MTU and iOS does not perform a GATT Read
  /// Long, so the Pi streams it as notifications (one network per notification,
  /// then a {"done":true} sentinel) when we subscribe. Subscribing to this
  /// encrypt-notify characteristic is also what triggers pairing on iOS.
  Future<List<WifiNetwork>> readNetworks() async {
    final networks = <WifiNetwork>[];
    final completer = Completer<List<WifiNetwork>>();

    final sub = _ssidChar!.onValueReceived.listen((bytes) {
      try {
        final obj = jsonDecode(utf8.decode(bytes));
        if (obj is! Map<String, dynamic>) return;
        if (obj['done'] == true) {
          if (!completer.isCompleted) completer.complete(List.of(networks));
          return;
        }
        final n = WifiNetwork.fromJson(obj);
        if (n.ssid.isNotEmpty) networks.add(n);
      } catch (_) {
        // Skip malformed frames.
      }
    });

    try {
      // Enable notifications to receive the streamed list.
      await _withEncryptionRetry(() => _ssidChar!.setNotifyValue(true));
      // Then read to TRIGGER the scan+stream. iOS may reuse a cached
      // subscription and skip re-issuing StartNotify on reconnect, so the
      // subscribe alone can't be relied on to kick the Pi — a read always does.
      // (The read returns the previous snapshot, which our listener ignores;
      // the fresh networks arrive as notifications.)
      await _withEncryptionRetry(() => _ssidChar!.read());
      // Fall back to whatever arrived if the sentinel is lost.
      return await completer.future.timeout(
        const Duration(seconds: 20),
        onTimeout: () => List.of(networks),
      );
    } finally {
      await sub.cancel();
    }
  }

  /// Subscribe to status notifications. Emits the current value first.
  Stream<ProvisioningStatus> statusStream() async* {
    await _withEncryptionRetry(() => _statusChar!.setNotifyValue(true));
    try {
      final initial = await _withEncryptionRetry(() => _statusChar!.read());
      yield ProvisioningStatus.fromBytes(initial);
    } catch (_) {
      // Ignore; notifications below will deliver state.
    }
    yield* _statusChar!.onValueReceived.map(ProvisioningStatus.fromBytes);
  }

  /// Write WiFi credentials. Pass a null/empty [psk] for open networks.
  Future<void> sendCredentials(String ssid, String? psk) async {
    final payload = <String, String>{'ssid': ssid};
    if (psk != null && psk.isNotEmpty) payload['psk'] = psk;
    await _credsChar!.write(utf8.encode(jsonEncode(payload)));
  }

  Future<void> dispose() async {
    try {
      await device.disconnect();
    } catch (_) {}
  }
}

class ProvisioningException implements Exception {
  final String message;
  const ProvisioningException(this.message);
  @override
  String toString() => message;
}
