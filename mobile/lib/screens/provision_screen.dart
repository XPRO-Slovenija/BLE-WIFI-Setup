import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../ble/provisioning_service.dart';
import '../models/provisioning_status.dart';
import '../models/wifi_network.dart';
import '../storage/credential_store.dart';
import '../widgets/logo_header.dart';

enum _Phase { connecting, ready, error }

class ProvisionScreen extends StatefulWidget {
  const ProvisionScreen({super.key, required this.device, this.title});

  final BluetoothDevice device;

  /// Display name (e.g. a local alias) for the app bar; falls back to the
  /// device's advertised name.
  final String? title;

  @override
  State<ProvisionScreen> createState() => _ProvisionScreenState();
}

class _ProvisionScreenState extends State<ProvisionScreen> {
  late final ProvisioningService _service;
  final CredentialStore _store = CredentialStore();
  StreamSubscription<ProvisioningStatus>? _statusSub;

  _Phase _phase = _Phase.connecting;
  String _errorText = '';
  List<WifiNetwork> _networks = const [];
  Set<String> _savedSsids = const {};

  static const String _manualValue = '__manual__';
  String? _selected; // SSID, or _manualValue, or null
  final _ssidCtrl = TextEditingController();
  final _pskCtrl = TextEditingController();
  bool _obscure = true;
  bool _sending = false;
  bool _rescanning = false;
  ProvisioningStatus? _status;

  // Remember what we last sent so we can persist it once the Pi confirms it
  // connected successfully.
  String? _lastSentSsid;
  String? _lastSentPsk;

  @override
  void initState() {
    super.initState();
    _service = ProvisioningService(widget.device);
    _connect();
  }

  Future<void> _connect() async {
    setState(() {
      _phase = _Phase.connecting;
      _errorText = '';
    });
    try {
      await _service.connectAndDiscover();
      _statusSub = _service.statusStream().listen((s) {
        setState(() => _status = s);
        if (s.state == ProvState.connected) _rememberOnSuccess();
      });
      final nets = await _service.readNetworks();
      final saved = await _store.savedSsids();
      if (!mounted) return;
      setState(() {
        _networks = nets;
        _savedSsids = saved;
        _phase = _Phase.ready;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _phase = _Phase.error;
        _errorText = e.toString();
      });
    }
  }

  bool get _isManual => _selected == _manualValue;

  bool get _selectedSecured {
    if (_isManual) return true;
    final match = _networks.where((n) => n.ssid == _selected);
    return match.isEmpty ? true : match.first.secured;
  }

  String get _effectiveSsid =>
      _isManual ? _ssidCtrl.text.trim() : (_selected ?? '');

  bool get _canSend =>
      !_sending && _effectiveSsid.isNotEmpty &&
      (!_selectedSecured || _pskCtrl.text.isNotEmpty);

  // Re-fetch the network list (re-triggers a scan on the Pi).
  Future<void> _rescan() async {
    setState(() => _rescanning = true);
    try {
      final nets = await _service.readNetworks();
      if (mounted) setState(() => _networks = nets);
    } catch (_) {
      // Leave the existing list; the user can try again.
    } finally {
      if (mounted) setState(() => _rescanning = false);
    }
  }

  // Pre-fill a saved password when a known secured network is selected.
  Future<void> _onSelectNetwork(String? value) async {
    setState(() {
      _selected = value;
      _pskCtrl.clear();
    });
    if (value == null || value == _manualValue) return;
    final saved = await _store.getPassword(value);
    if (saved != null && mounted && _selected == value) {
      setState(() => _pskCtrl.text = saved);
    }
  }

  // Persist the credentials of the last send once the Pi reports it connected.
  void _rememberOnSuccess() {
    final ssid = _lastSentSsid;
    final psk = _lastSentPsk;
    if (ssid == null || psk == null || psk.isEmpty) return;
    _store.savePassword(ssid, psk);
    if (mounted) setState(() => _savedSsids = {..._savedSsids, ssid});
  }

  Future<void> _send() async {
    FocusScope.of(context).unfocus();
    _lastSentSsid = _effectiveSsid;
    _lastSentPsk = _selectedSecured ? _pskCtrl.text : null;
    setState(() => _sending = true);
    try {
      await _service.sendCredentials(
        _effectiveSsid,
        _selectedSecured ? _pskCtrl.text : null,
      );
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to send: $e')),
        );
      }
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  @override
  void dispose() {
    _statusSub?.cancel();
    _service.dispose();
    _ssidCtrl.dispose();
    _pskCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final name = widget.title?.isNotEmpty == true
        ? widget.title!
        : (widget.device.advName.isNotEmpty
            ? widget.device.advName
            : (widget.device.platformName.isNotEmpty
                ? widget.device.platformName
                : widget.device.remoteId.str));
    return Scaffold(
      appBar: AppBar(
        title: Text(name),
        actions: [
          if (_phase == _Phase.ready)
            IconButton(
              tooltip: 'Rescan networks',
              icon: _rescanning
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.refresh),
              onPressed: _rescanning ? null : _rescan,
            ),
        ],
      ),
      body: Column(
        children: [
          const LogoHeader(),
          Expanded(
            child: switch (_phase) {
        _Phase.connecting => const _Centered(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                CircularProgressIndicator(),
                SizedBox(height: 16),
                Text('Connecting and pairing…'),
              ],
            ),
          ),
        _Phase.error => _Centered(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.error_outline, size: 56, color: Colors.red),
                const SizedBox(height: 16),
                Text(_errorText, textAlign: TextAlign.center),
                const SizedBox(height: 24),
                FilledButton(onPressed: _connect, child: const Text('Retry')),
              ],
            ),
          ),
        _Phase.ready => _buildForm(context),
            },
          ),
        ],
      ),
    );
  }

  Widget _buildForm(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          _networks.isEmpty
              ? 'No networks found yet — tap the ⟳ button to rescan.'
              : 'Choose a WiFi network for this Pi to join.',
          style: Theme.of(context).textTheme.bodyMedium,
        ),
        const SizedBox(height: 20),
        DropdownButtonFormField<String>(
          initialValue: _selected,
          isExpanded: true,
          decoration: const InputDecoration(
            labelText: 'Network',
            border: OutlineInputBorder(),
          ),
          items: [
            ..._networks.map((n) => DropdownMenuItem(
                  value: n.ssid,
                  child: Row(
                    children: [
                      Icon(n.secured ? Icons.lock : Icons.lock_open, size: 16),
                      const SizedBox(width: 8),
                      Expanded(
                          child: Text(n.ssid, overflow: TextOverflow.ellipsis)),
                      if (_savedSsids.contains(n.ssid)) ...[
                        const Icon(Icons.history, size: 16),
                        const SizedBox(width: 6),
                      ],
                      _SignalIcon(signal: n.signal),
                    ],
                  ),
                )),
            const DropdownMenuItem(
              value: _manualValue,
              child: Row(children: [
                Icon(Icons.edit, size: 16),
                SizedBox(width: 8),
                Text('Other network…'),
              ]),
            ),
          ],
          onChanged: _onSelectNetwork,
        ),
        if (_isManual) ...[
          const SizedBox(height: 16),
          TextField(
            controller: _ssidCtrl,
            decoration: const InputDecoration(
              labelText: 'Network name (SSID)',
              border: OutlineInputBorder(),
            ),
            onChanged: (_) => setState(() {}),
          ),
        ],
        if (_selected != null && _selectedSecured) ...[
          const SizedBox(height: 16),
          TextField(
            controller: _pskCtrl,
            obscureText: _obscure,
            decoration: InputDecoration(
              labelText: 'Password',
              border: const OutlineInputBorder(),
              suffixIcon: IconButton(
                icon: Icon(_obscure ? Icons.visibility : Icons.visibility_off),
                onPressed: () => setState(() => _obscure = !_obscure),
              ),
            ),
            onChanged: (_) => setState(() {}),
          ),
        ],
        const SizedBox(height: 24),
        FilledButton.icon(
          onPressed: _canSend ? _send : null,
          icon: _sending
              ? const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Icon(Icons.wifi),
          label: const Text('Send WiFi settings'),
        ),
        if (_status != null) ...[
          const SizedBox(height: 24),
          _StatusCard(status: _status!),
        ],
      ],
    );
  }
}

class _StatusCard extends StatelessWidget {
  const _StatusCard({required this.status});
  final ProvisioningStatus status;

  @override
  Widget build(BuildContext context) {
    final (IconData icon, Color color, String text) = switch (status.state) {
      ProvState.connecting => (
          Icons.sync,
          Colors.orange,
          'Connecting to ${status.ssid}…'
        ),
      ProvState.connected => (
          Icons.check_circle,
          Colors.green,
          'Connected${status.ip.isNotEmpty ? ' — ${status.ip}' : ''}'
        ),
      ProvState.failed => (
          Icons.error,
          Colors.red,
          status.error.isNotEmpty ? status.error : 'Connection failed'
        ),
      _ => (Icons.info_outline, Colors.grey, 'Idle'),
    };
    return Card(
      child: ListTile(
        leading: Icon(icon, color: color),
        title: Text(text),
        subtitle: status.state == ProvState.connected && status.ssid.isNotEmpty
            ? Text(status.ssid)
            : null,
      ),
    );
  }
}

class _SignalIcon extends StatelessWidget {
  const _SignalIcon({required this.signal});
  final int signal;

  @override
  Widget build(BuildContext context) {
    final IconData icon = signal >= 66
        ? Icons.wifi
        : signal >= 33
            ? Icons.wifi_2_bar
            : Icons.wifi_1_bar;
    return Icon(icon, size: 16);
  }
}

class _Centered extends StatelessWidget {
  const _Centered({required this.child});
  final Widget child;
  @override
  Widget build(BuildContext context) =>
      Center(child: Padding(padding: const EdgeInsets.all(32), child: child));
}
