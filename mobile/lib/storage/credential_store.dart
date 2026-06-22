import 'package:flutter_secure_storage/flutter_secure_storage.dart';

/// Remembers WiFi passwords for networks the user has successfully provisioned,
/// so they can be pre-filled next time. Backed by the platform secure store
/// (iOS Keychain / Android Keystore) since these are credentials.
class CredentialStore {
  static const _prefix = 'wifi_psk:';
  final FlutterSecureStorage _storage = const FlutterSecureStorage();

  Future<void> savePassword(String ssid, String psk) =>
      _storage.write(key: '$_prefix$ssid', value: psk);

  Future<String?> getPassword(String ssid) =>
      _storage.read(key: '$_prefix$ssid');

  Future<void> remove(String ssid) => _storage.delete(key: '$_prefix$ssid');

  /// SSIDs we have a saved password for (used to badge the network list).
  Future<Set<String>> savedSsids() async {
    final all = await _storage.readAll();
    return all.keys
        .where((k) => k.startsWith(_prefix))
        .map((k) => k.substring(_prefix.length))
        .toSet();
  }
}
