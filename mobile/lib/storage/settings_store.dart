import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Non-sensitive app settings persisted in shared_preferences: the theme mode
/// and per-device friendly names (keyed by BLE remote id). WiFi passwords are
/// NOT stored here — those live in the secure store (see CredentialStore).
class SettingsStore {
  static const _kThemeMode = 'theme_mode';
  static const _kAliases = 'device_aliases';

  Future<ThemeMode> themeMode() async {
    final prefs = await SharedPreferences.getInstance();
    switch (prefs.getString(_kThemeMode)) {
      case 'light':
        return ThemeMode.light;
      case 'dark':
        return ThemeMode.dark;
      default:
        return ThemeMode.system;
    }
  }

  Future<void> setThemeMode(ThemeMode mode) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kThemeMode, mode.name);
  }

  Future<Map<String, String>> aliases() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_kAliases);
    if (raw == null) return {};
    try {
      final decoded = jsonDecode(raw) as Map<String, dynamic>;
      return decoded.map((k, v) => MapEntry(k, v.toString()));
    } catch (_) {
      return {};
    }
  }

  Future<void> saveAliases(Map<String, String> aliases) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kAliases, jsonEncode(aliases));
  }
}
