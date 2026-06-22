#include "wifi/wpa_supplicant.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <map>
#include <sstream>
#include <thread>

#include "util/log.h"
#include "util/subprocess.h"

namespace wifi {
namespace {

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream is(s);
  while (std::getline(is, cur, sep)) out.push_back(cur);
  return out;
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Parse "key=value" lines (wpa_cli status output) into a map.
std::map<std::string, std::string> parse_kv(const std::string& text) {
  std::map<std::string, std::string> m;
  for (const auto& line : split(text, '\n')) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    m[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
  }
  return m;
}

class WpaSupplicantBackend : public Backend {
 public:
  WpaSupplicantBackend() : iface_(detect_iface()) {}

  const char* name() const override { return "wpa_supplicant"; }

  std::vector<Network> scan() override {
    std::vector<Network> nets;
    if (iface_.empty()) return nets;

    util::run({"wpa_cli", "-i", iface_, "scan"});
    // Give the radio a moment, then read whatever results are available.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    auto r = util::run({"wpa_cli", "-i", iface_, "scan_results"});
    if (r.exit_code != 0) {
      logging::warn("wpa_cli scan_results failed: %s", trim(r.err).c_str());
      return nets;
    }

    // Format: header line then rows of: bssid \t freq \t signal(dBm) \t flags \t ssid
    bool first = true;
    for (const auto& line : split(r.out, '\n')) {
      if (first) { first = false; continue; }
      if (line.empty()) continue;
      auto f = split(line, '\t');
      if (f.size() < 5) continue;
      const std::string& ssid = f[4];
      if (ssid.empty()) continue;
      Network n;
      n.ssid = ssid;
      int dbm = std::atoi(f[2].c_str());
      n.signal = std::max(0, std::min(100, 2 * (dbm + 100))); // rough dBm -> %
      const std::string& flags = f[3];
      n.secured = flags.find("WPA") != std::string::npos ||
                  flags.find("RSN") != std::string::npos ||
                  flags.find("WEP") != std::string::npos;
      auto it = std::find_if(nets.begin(), nets.end(),
                             [&](const Network& e) { return e.ssid == n.ssid; });
      if (it == nets.end()) nets.push_back(n);
      else if (n.signal > it->signal) *it = n;
    }
    std::sort(nets.begin(), nets.end(),
              [](const Network& a, const Network& b) { return a.signal > b.signal; });
    return nets;
  }

  bool connect(const std::string& ssid, const std::string& psk,
               std::string& error) override {
    if (iface_.empty()) { error = "no wifi interface"; return false; }

    auto add = util::run({"wpa_cli", "-i", iface_, "add_network"});
    std::string id = trim(add.out);
    // add_network echoes the new network id on its own line.
    {
      auto lines = split(id, '\n');
      if (!lines.empty()) id = trim(lines.back());
    }
    if (id.empty() || !std::all_of(id.begin(), id.end(), ::isdigit)) {
      error = "add_network failed";
      return false;
    }

    // wpa expects quoted string values; we pass the quotes as part of the argv
    // value. Control characters are rejected upstream, so this cannot inject
    // extra config directives.
    if (!set_network(id, "ssid", "\"" + ssid + "\"", error)) return false;
    if (psk.empty()) {
      if (!set_network(id, "key_mgmt", "NONE", error)) return false;
    } else {
      if (!set_network(id, "psk", "\"" + psk + "\"", error)) return false;
    }

    if (!ok(util::run({"wpa_cli", "-i", iface_, "enable_network", id}))) {
      error = "enable_network failed"; return false;
    }
    if (!ok(util::run({"wpa_cli", "-i", iface_, "select_network", id}))) {
      error = "select_network failed"; return false;
    }
    util::run({"wpa_cli", "-i", iface_, "save_config"});

    // Poll for association (up to ~25s).
    for (int i = 0; i < 25; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto m = parse_kv(util::run({"wpa_cli", "-i", iface_, "status"}).out);
      if (m["wpa_state"] == "COMPLETED") {
        ensure_dhcp();
        return true;
      }
    }
    error = "association timed out";
    return false;
  }

  Status status() override {
    Status s;
    s.state = "idle";
    if (iface_.empty()) return s;
    auto m = parse_kv(util::run({"wpa_cli", "-i", iface_, "status"}).out);
    s.ssid = m.count("ssid") ? m["ssid"] : "";
    if (m["wpa_state"] == "COMPLETED") {
      s.ip = m.count("ip_address") ? m["ip_address"] : "";
      s.state = s.ip.empty() ? "connecting" : "connected";
    } else if (!m["wpa_state"].empty() && m["wpa_state"] != "DISCONNECTED" &&
               m["wpa_state"] != "INACTIVE") {
      s.state = "connecting";
    }
    return s;
  }

 private:
  std::string iface_;

  bool ok(const util::ProcResult& r) {
    return r.exit_code == 0 && trim(r.out).find("FAIL") == std::string::npos;
  }

  bool set_network(const std::string& id, const std::string& key,
                   const std::string& value, std::string& error) {
    if (!ok(util::run({"wpa_cli", "-i", iface_, "set_network", id, key, value}))) {
      error = "set_network " + key + " failed";
      return false;
    }
    return true;
  }

  // On wpa_supplicant-only systems dhcpcd usually reacts to association on its
  // own, but nudge it so an address is acquired promptly.
  void ensure_dhcp() {
    auto r = util::run({"dhcpcd", "-n", iface_});
    if (r.exit_code != 0) {
      // dhcpcd may be absent; fall back to dhclient if present.
      util::run({"dhclient", iface_});
    }
  }

  // Pick the first wireless interface from `iw dev`.
  static std::string detect_iface() {
    auto r = util::run({"iw", "dev"});
    for (const auto& line : split(r.out, '\n')) {
      auto t = trim(line);
      const std::string key = "Interface ";
      if (t.rfind(key, 0) == 0) return t.substr(key.size());
    }
    return "wlan0"; // sensible default
  }
};

} // namespace

std::unique_ptr<Backend> make_wpa_supplicant() {
  return std::make_unique<WpaSupplicantBackend>();
}

} // namespace wifi
