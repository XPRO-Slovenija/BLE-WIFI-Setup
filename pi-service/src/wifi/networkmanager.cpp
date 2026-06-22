#include "wifi/networkmanager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <thread>

#include "util/log.h"
#include "util/subprocess.h"

namespace wifi {
namespace {

// Split a string on `sep`, no escape handling.
std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream is(s);
  while (std::getline(is, cur, sep)) out.push_back(cur);
  return out;
}

// nmcli terse mode (`-t`) escapes the field separator and backslash, e.g. an
// SSID "a:b" comes back as "a\:b". Split a terse line on UNescaped `:` and
// unescape each field so callers see the real values.
std::vector<std::string> split_terse(const std::string& line) {
  std::vector<std::string> fields;
  std::string cur;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\\' && i + 1 < line.size()) {
      cur.push_back(line[++i]);
    } else if (c == ':') {
      fields.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  fields.push_back(cur);
  return fields;
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Map a raw nmcli failure into a short, user-facing message. The raw text is
// still logged for debugging.
std::string friendly_error(const std::string& raw) {
  std::string low = raw;
  std::transform(low.begin(), low.end(), low.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto has = [&](const char* s) { return low.find(s) != std::string::npos; };
  if (has("secrets were required") || has("no secrets") ||
      has("incorrect password")) {
    return "Incorrect WiFi password.";
  }
  if (has("no network with ssid") || has("not found") || has("no suitable")) {
    return "Network not found or out of range.";
  }
  if (has("timeout") || has("timed out")) {
    return "Timed out connecting to the network.";
  }
  return "Could not connect to the network.";
}

class NetworkManagerBackend : public Backend {
 public:
  const char* name() const override { return "NetworkManager"; }

  std::vector<Network> scan() override {
    ensure_radio_on();
    // Force a real scan. NOTE: `nmcli ... wifi list --rescan yes` does NOT scan
    // while wlan0 is connected — it returns the cached list (often just the
    // current AP). An explicit `device wifi rescan` does. If a scan just ran it
    // returns "scanning not allowed"; that's fine — we poll the existing cache.
    util::run({"nmcli", "device", "wifi", "rescan"});

    // Results populate asynchronously over a few seconds, and the timing varies
    // (slower right after a network switch or under WiFi/BT contention), so a
    // fixed delay is racy. Poll instead: keep the largest list seen and stop
    // once it stops growing, capped at ~9s. Runs on a worker thread, so it never
    // blocks the GATT main loop.
    std::vector<Network> best;
    int stable = 0;
    for (int i = 0; i < 9; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto r = util::run({"nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY",
                          "device", "wifi", "list", "--rescan", "no"});
      if (r.exit_code != 0) continue;
      auto nets = parse_list(r.out);
      if (nets.size() > best.size()) {
        best = std::move(nets);
        stable = 0;
      } else {
        ++stable;
      }
      // A real list (more than just the current AP) that has settled => done.
      if (best.size() > 1 && stable >= 2) break;
    }
    logging::info("scan found %zu networks", best.size());
    return best;
  }

  // Parse terse `nmcli ... wifi list` output, dedup by SSID (strongest signal),
  // sorted by signal descending.
  std::vector<Network> parse_list(const std::string& out) {
    std::vector<Network> nets;
    for (const auto& line : split(out, '\n')) {
      if (line.empty()) continue;
      auto f = split_terse(line);
      if (f.size() < 3) continue;
      if (f[0].empty()) continue; // hidden SSID
      Network n;
      n.ssid = f[0];
      n.signal = std::atoi(f[1].c_str());
      n.secured = !(trim(f[2]).empty() || trim(f[2]) == "--");
      auto it = std::find_if(nets.begin(), nets.end(),
                             [&](const Network& e) { return e.ssid == n.ssid; });
      if (it == nets.end()) {
        nets.push_back(n);
      } else if (n.signal > it->signal) {
        *it = n;
      }
    }
    std::sort(nets.begin(), nets.end(),
              [](const Network& a, const Network& b) { return a.signal > b.signal; });
    return nets;
  }

  bool connect(const std::string& ssid, const std::string& psk,
               std::string& error) override {
    ensure_radio_on();
    // Delete any existing profile for this SSID first. A leftover/partial
    // profile (e.g. from an interrupted earlier attempt) makes `device wifi
    // connect` fail with "802-11-wireless-security.key-mgmt: property is
    // missing". Recreating it fresh avoids that. `id <ssid>` matches the
    // auto-named profiles `device wifi connect` creates; ignore "no such
    // profile" errors.
    util::run({"nmcli", "connection", "delete", "id", ssid});
    std::vector<std::string> args = {"nmcli", "-w", "25", "device", "wifi",
                                     "connect", ssid};
    if (!psk.empty()) {
      args.push_back("password");
      args.push_back(psk);
    }
    auto r = util::run(args);
    if (r.exit_code == 0) return true;
    std::string raw = trim(r.err);
    if (raw.empty()) raw = trim(r.out);
    logging::warn("nmcli connect failed: %s", raw.c_str());
    error = friendly_error(raw);
    return false;
  }

  Status status() override {
    Status s;
    s.state = "idle";
    std::string iface = wifi_iface();
    if (iface.empty()) return s;

    auto st = util::run({"nmcli", "-g", "GENERAL.STATE", "device", "show", iface});
    std::string state = trim(st.out); // e.g. "100 (connected)"
    bool connected = state.rfind("100", 0) == 0;

    auto cn = util::run({"nmcli", "-g", "GENERAL.CONNECTION", "device", "show", iface});
    s.ssid = trim(cn.out);

    if (connected) {
      auto ip = util::run({"nmcli", "-g", "IP4.ADDRESS", "device", "show", iface});
      std::string addr = trim(ip.out);
      auto slash = addr.find('/');
      if (slash != std::string::npos) addr = addr.substr(0, slash);
      // IP4.ADDRESS can be multi-line; keep the first.
      auto nl = addr.find('\n');
      if (nl != std::string::npos) addr = addr.substr(0, nl);
      s.ip = addr;
      s.state = s.ip.empty() ? "connecting" : "connected";
    }
    return s;
  }

 private:
  // Make sure the WiFi radio is usable. A fresh Pi often has WiFi rfkill
  // soft-blocked (no country set) and the NM radio switch off, which leaves
  // wlan0 "unavailable" and scans empty. These are idempotent and cheap.
  void ensure_radio_on() {
    util::run({"rfkill", "unblock", "wifi"});
    util::run({"nmcli", "radio", "wifi", "on"});
  }

  // First wifi-type device reported by nmcli (usually wlan0).
  std::string wifi_iface() {
    auto r = util::run({"nmcli", "-t", "-f", "DEVICE,TYPE", "device", "status"});
    for (const auto& line : split(r.out, '\n')) {
      auto f = split_terse(line);
      if (f.size() >= 2 && f[1] == "wifi") return f[0];
    }
    return "";
  }
};

} // namespace

std::unique_ptr<Backend> make_networkmanager() {
  return std::make_unique<NetworkManagerBackend>();
}

} // namespace wifi
