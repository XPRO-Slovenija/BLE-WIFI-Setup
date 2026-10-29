#pragma once
#include <string>

// Runtime configuration: built-in defaults, overridden by a key=value config
// file, overridden in turn by command-line flags.
namespace config {

#ifndef RPI_BLE_PROVISION_VERSION
#define RPI_BLE_PROVISION_VERSION "dev"
#endif
constexpr const char* kVersion = RPI_BLE_PROVISION_VERSION;

// Used when no config file / flag sets one. Must match the mobile app's default.
constexpr const char* kDefaultServiceUUID = "7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00";
constexpr const char* kDefaultConfigPath = "/etc/rpi-ble-provision.conf";

struct Config {
  // The service UUID ends in "00"; the characteristics are the same UUID with
  // the last byte set to 01 (SSID list), 02 (credentials), 03 (status).
  std::string service_uuid;
  std::string ssid_list_uuid;
  std::string creds_uuid;
  std::string status_uuid;

  // Advertised BLE name. Empty means "derive rpi-prov-<serial suffix>".
  std::string device_name;
};

enum class ParseResult { Ok, Exit, Error };

// Parse argv (and the config file it points to, or the default one if present)
// into `out`. Returns Exit after --help/--version, Error on invalid input (already logged).
ParseResult parse(int argc, char** argv, Config& out);

} // namespace config
