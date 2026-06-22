#pragma once
#include <memory>
#include <string>
#include <vector>

namespace wifi {

struct Network {
  std::string ssid;
  int signal = 0;     // 0..100 (%)
  bool secured = true;
};

struct Status {
  std::string state;  // "idle" | "connecting" | "connected" | "failed"
  std::string ip;     // current IPv4 address when connected, else empty
  std::string ssid;   // SSID we are connected/connecting to, else empty
  std::string error;  // human-readable reason when state == "failed"
};

// Abstraction over the host's network manager. Concrete backends shell out to
// nmcli (NetworkManager) or wpa_cli/iw (wpa_supplicant). All implementations
// MUST pass user-supplied SSID/PSK as argv (never through a shell).
class Backend {
 public:
  virtual ~Backend() = default;

  // Backend name for logging ("NetworkManager" / "wpa_supplicant").
  virtual const char* name() const = 0;

  // Visible networks. Uses cached scan results where possible to stay fast
  // (a BLE ReadValue should not block for seconds).
  virtual std::vector<Network> scan() = 0;

  // Attempt to join `ssid` with `psk` (empty psk => open network). Blocks until
  // the join succeeds or fails; intended to be called off the GLib main thread.
  // On failure returns false and fills `error`.
  virtual bool connect(const std::string& ssid, const std::string& psk,
                       std::string& error) = 0;

  // Current connection status.
  virtual Status status() = 0;
};

// Detect the active network stack and return the matching backend, or nullptr
// if neither NetworkManager nor wpa_supplicant appears usable.
std::unique_ptr<Backend> detect();

} // namespace wifi
