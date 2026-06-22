import 'dart:async';
import 'dart:io' show Platform;

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../ble/ble_ids.dart';
import '../ble/ble_permissions.dart';
import '../settings_controller.dart';
import '../widgets/logo_header.dart';
import 'provision_screen.dart';

class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key, required this.settings});

  final SettingsController settings;

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  final Map<DeviceIdentifier, ScanResult> _results = {};
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  BluetoothAdapterState _adapterState = BluetoothAdapterState.unknown;
  bool _scanning = false;
  bool _permissionDenied = false;

  @override
  void initState() {
    super.initState();
    _adapterSub = FlutterBluePlus.adapterState.listen((s) {
      setState(() => _adapterState = s);
      if (s == BluetoothAdapterState.on && _results.isEmpty && !_scanning) {
        _startScan();
      }
    });
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      setState(() {
        for (final r in results) {
          if (_isPi(r)) _results[r.device.remoteId] = r;
        }
      });
    });
    _init();
  }

  Future<void> _init() async {
    final ok = await requestBlePermissions();
    if (!ok) {
      setState(() => _permissionDenied = true);
      return;
    }
    if (FlutterBluePlus.adapterStateNow == BluetoothAdapterState.on) {
      _startScan();
    }
  }

  bool _isPi(ScanResult r) {
    if (r.advertisementData.serviceUuids.contains(BleIds.service)) return true;
    return r.advertisementData.advName.startsWith(BleIds.namePrefix);
  }

  Future<void> _startScan() async {
    if (_scanning) return;
    setState(() {
      _scanning = true;
      _results.clear();
    });
    try {
      await FlutterBluePlus.startScan(
        withServices: [BleIds.service],
        timeout: const Duration(seconds: 12),
      );
      await FlutterBluePlus.isScanning.where((s) => s == false).first;
    } catch (_) {
      // surfaced via empty list / retry button
    }
    if (mounted) setState(() => _scanning = false);
  }

  // The Pi's own advertised name (or id), ignoring any local alias.
  String _bleName(ScanResult r) {
    final adv = r.advertisementData.advName;
    if (adv.isNotEmpty) return adv;
    if (r.device.platformName.isNotEmpty) return r.device.platformName;
    return r.device.remoteId.str;
  }

  // What we show: local alias if the user set one, else the BLE name.
  String _displayName(ScanResult r) {
    return widget.settings.aliasFor(r.device.remoteId.str) ?? _bleName(r);
  }

  Future<void> _rename(ScanResult r) async {
    final id = r.device.remoteId.str;
    final controller = TextEditingController(
      text: widget.settings.aliasFor(id) ?? '',
    );
    final name = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Rename Pi'),
        content: TextField(
          controller: controller,
          autofocus: true,
          decoration: InputDecoration(
            labelText: 'Name',
            hintText: _bleName(r),
            border: const OutlineInputBorder(),
          ),
          onSubmitted: (v) => Navigator.pop(ctx, v),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, controller.text),
            child: const Text('Save'),
          ),
        ],
      ),
    );
    if (name != null) await widget.settings.setAlias(id, name);
  }

  Future<void> _forget(ScanResult r) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text('Forget "${_displayName(r)}"?'),
        content: const Text(
          'This removes its saved name and Bluetooth pairing from this phone.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Forget'),
          ),
        ],
      ),
    );
    if (confirm != true) return;

    await widget.settings.forget(r.device.remoteId.str);
    try {
      await r.device.removeBond(); // Android only; throws on iOS
    } catch (_) {}
    try {
      await r.device.disconnect();
    } catch (_) {}
    if (!mounted) return;
    setState(() => _results.remove(r.device.remoteId));
    if (Platform.isIOS) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text(
            'Also forget this device in iOS Settings ▸ Bluetooth to remove pairing.',
          ),
        ),
      );
    }
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _adapterSub?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  IconData _themeIcon(ThemeMode m) => switch (m) {
    ThemeMode.light => Icons.light_mode,
    ThemeMode.dark => Icons.dark_mode,
    ThemeMode.system => Icons.brightness_auto,
  };

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Set up a Raspberry Pi\'s WIFI'),
        actions: [
          PopupMenuButton<ThemeMode>(
            icon: Icon(_themeIcon(widget.settings.themeMode)),
            tooltip: 'Appearance',
            onSelected: widget.settings.setThemeMode,
            itemBuilder: (_) => [
              for (final m in ThemeMode.values)
                CheckedPopupMenuItem(
                  value: m,
                  checked: widget.settings.themeMode == m,
                  child: Text(switch (m) {
                    ThemeMode.system => 'System',
                    ThemeMode.light => 'Light',
                    ThemeMode.dark => 'Dark',
                  }),
                ),
            ],
          ),
          if (_scanning)
            const Padding(
              padding: EdgeInsets.only(right: 16),
              child: Center(
                child: SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                ),
              ),
            )
          else
            IconButton(
              tooltip: 'Scan',
              icon: const Icon(Icons.refresh),
              onPressed: _adapterState == BluetoothAdapterState.on
                  ? _startScan
                  : null,
            ),
        ],
      ),
      body: Column(
        children: [
          const LogoHeader(),
          Expanded(child: _buildBody()),
        ],
      ),
    );
  }

  Widget _buildBody() {
    if (_permissionDenied) {
      return _Message(
        icon: Icons.lock_outline,
        title: 'Bluetooth permission needed',
        subtitle: 'Allow Bluetooth access in Settings to find your Pi.',
        action: FilledButton(
          onPressed: () => openAppSettings(),
          child: const Text('Open Settings'),
        ),
      );
    }
    if (_adapterState != BluetoothAdapterState.on) {
      return const _Message(
        icon: Icons.bluetooth_disabled,
        title: 'Bluetooth is off',
        subtitle: 'Turn on Bluetooth to find nearby Raspberry Pis.',
      );
    }
    if (_results.isEmpty) {
      return _Message(
        icon: Icons.bluetooth_searching,
        title: _scanning ? 'Searching…' : 'No devices found',
        subtitle:
            'Make sure the Pi is powered on and running the '
            'provisioning service.',
        action: _scanning
            ? null
            : FilledButton(
                onPressed: _startScan,
                child: const Text('Scan again'),
              ),
      );
    }

    final items = _results.values.toList();
    return ListView.separated(
      itemCount: items.length,
      separatorBuilder: (_, index) => const Divider(height: 1),
      itemBuilder: (context, i) {
        final r = items[i];
        final aliased = widget.settings.aliasFor(r.device.remoteId.str) != null;
        return ListTile(
          leading: const CircleAvatar(child: Icon(Icons.developer_board)),
          title: Text(_displayName(r)),
          subtitle: Text(
            aliased
                ? '${_bleName(r)} • ${r.rssi} dBm'
                : '${r.device.remoteId.str} • ${r.rssi} dBm',
          ),
          trailing: PopupMenuButton<String>(
            onSelected: (v) => v == 'rename' ? _rename(r) : _forget(r),
            itemBuilder: (_) => const [
              PopupMenuItem(value: 'rename', child: Text('Rename')),
              PopupMenuItem(value: 'forget', child: Text('Forget')),
            ],
          ),
          onTap: () async {
            final navigator = Navigator.of(context);
            await FlutterBluePlus.stopScan();
            if (!mounted) return;
            navigator.push(
              MaterialPageRoute(
                builder: (_) =>
                    ProvisionScreen(device: r.device, title: _displayName(r)),
              ),
            );
          },
        );
      },
    );
  }
}

class _Message extends StatelessWidget {
  const _Message({
    required this.icon,
    required this.title,
    required this.subtitle,
    this.action,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 64, color: Theme.of(context).colorScheme.primary),
            const SizedBox(height: 16),
            Text(title, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 8),
            Text(subtitle, textAlign: TextAlign.center),
            if (action != null) ...[const SizedBox(height: 24), action!],
          ],
        ),
      ),
    );
  }
}
