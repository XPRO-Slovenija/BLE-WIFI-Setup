import 'package:flutter_test/flutter_test.dart';
import 'package:rpi_provisioner/models/wifi_network.dart';
import 'package:rpi_provisioner/models/provisioning_status.dart';

void main() {
  test('WifiNetwork parses JSON', () {
    final n = WifiNetwork.fromJson({'ssid': 'Home', 'signal': 80, 'secured': true});
    expect(n.ssid, 'Home');
    expect(n.signal, 80);
    expect(n.secured, true);
  });

  test('ProvisioningStatus parses status bytes', () {
    final bytes = '{"state":"connected","ip":"192.168.1.5","ssid":"Home","error":""}'.codeUnits;
    final s = ProvisioningStatus.fromBytes(bytes);
    expect(s.state, ProvState.connected);
    expect(s.ip, '192.168.1.5');
  });

  test('ProvisioningStatus tolerates garbage', () {
    expect(ProvisioningStatus.fromBytes('not json'.codeUnits).state, ProvState.unknown);
  });
}
