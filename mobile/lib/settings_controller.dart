import 'package:flutter/material.dart';

import 'storage/settings_store.dart';

/// Holds app settings in memory and notifies listeners on change, so the app
/// (theme) and the scan list (device aliases) rebuild reactively. Changes are
/// persisted to [SettingsStore] in the background.
class SettingsController extends ChangeNotifier {
  SettingsController(this._store, this._themeMode, this._aliases);

  final SettingsStore _store;
  ThemeMode _themeMode;
  final Map<String, String> _aliases;

  static Future<SettingsController> load() async {
    final store = SettingsStore();
    final mode = await store.themeMode();
    final aliases = await store.aliases();
    return SettingsController(store, mode, aliases);
  }

  ThemeMode get themeMode => _themeMode;

  Future<void> setThemeMode(ThemeMode mode) async {
    if (mode == _themeMode) return;
    _themeMode = mode;
    notifyListeners();
    await _store.setThemeMode(mode);
  }

  /// Friendly name for a device, or null if it has no alias.
  String? aliasFor(String remoteId) => _aliases[remoteId];

  Future<void> setAlias(String remoteId, String name) async {
    final trimmed = name.trim();
    if (trimmed.isEmpty) {
      return forget(remoteId);
    }
    _aliases[remoteId] = trimmed;
    notifyListeners();
    await _store.saveAliases(_aliases);
  }

  /// Forget a device's app-side data (its alias).
  Future<void> forget(String remoteId) async {
    if (_aliases.remove(remoteId) == null) return;
    notifyListeners();
    await _store.saveAliases(_aliases);
  }
}
