import 'dart:convert';

enum ProvState { idle, connecting, connected, failed, unknown }

/// Decoded payload of the Pi's status characteristic.
class ProvisioningStatus {
  final ProvState state;
  final String ip;
  final String ssid;
  final String error;

  const ProvisioningStatus({
    required this.state,
    this.ip = '',
    this.ssid = '',
    this.error = '',
  });

  static const ProvisioningStatus idle = ProvisioningStatus(state: ProvState.idle);

  factory ProvisioningStatus.fromBytes(List<int> bytes) {
    try {
      final json = jsonDecode(utf8.decode(bytes)) as Map<String, dynamic>;
      return ProvisioningStatus(
        state: _parseState(json['state'] as String?),
        ip: (json['ip'] ?? '') as String,
        ssid: (json['ssid'] ?? '') as String,
        error: (json['error'] ?? '') as String,
      );
    } catch (_) {
      return const ProvisioningStatus(state: ProvState.unknown);
    }
  }

  static ProvState _parseState(String? s) {
    switch (s) {
      case 'idle':
        return ProvState.idle;
      case 'connecting':
        return ProvState.connecting;
      case 'connected':
        return ProvState.connected;
      case 'failed':
        return ProvState.failed;
      default:
        return ProvState.unknown;
    }
  }
}
