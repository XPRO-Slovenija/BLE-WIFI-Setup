/// One visible WiFi network as reported by the Pi's SSID-list characteristic.
class WifiNetwork {
  final String ssid;
  final int signal; // 0..100 (%)
  final bool secured;

  const WifiNetwork({
    required this.ssid,
    required this.signal,
    required this.secured,
  });

  factory WifiNetwork.fromJson(Map<String, dynamic> json) {
    return WifiNetwork(
      ssid: (json['ssid'] ?? '') as String,
      signal: (json['signal'] ?? 0) as int,
      secured: (json['secured'] ?? true) as bool,
    );
  }
}
