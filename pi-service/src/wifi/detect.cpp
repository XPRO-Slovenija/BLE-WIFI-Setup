#include "wifi/backend.h"

#include "util/log.h"
#include "util/subprocess.h"
#include "wifi/networkmanager.h"
#include "wifi/wpa_supplicant.h"

namespace wifi {

namespace {
bool unit_active(const char* unit) {
  return util::run({"systemctl", "is-active", "--quiet", unit}).exit_code == 0;
}
bool tool_present(const char* tool, const char* version_flag) {
  return util::run({tool, version_flag}).exit_code == 0;
}
} // namespace

std::unique_ptr<Backend> detect() {
  // Prefer whichever manager systemd reports as active.
  if (unit_active("NetworkManager")) {
    logging::info("wifi backend: NetworkManager (service active)");
    return make_networkmanager();
  }
  if (unit_active("wpa_supplicant") || unit_active("wpa_supplicant@wlan0")) {
    logging::info("wifi backend: wpa_supplicant (service active)");
    return make_wpa_supplicant();
  }

  // Neither service reported active: fall back to whichever CLI exists.
  if (tool_present("nmcli", "--version")) {
    logging::info("wifi backend: NetworkManager (nmcli present)");
    return make_networkmanager();
  }
  if (tool_present("wpa_cli", "-v")) {
    logging::info("wifi backend: wpa_supplicant (wpa_cli present)");
    return make_wpa_supplicant();
  }

  logging::error("no usable wifi backend found (neither NetworkManager nor wpa_supplicant)");
  return nullptr;
}

} // namespace wifi
