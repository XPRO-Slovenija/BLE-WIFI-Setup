#include "config.h"

#include <getopt.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include "util/log.h"

namespace config {
namespace {

// Longest name that fits a 31-byte scan-response AD structure. BlueZ truncates
// anything longer, so we warn rather than fail.
constexpr size_t kMaxAdvNameBytes = 29;

void print_usage(const char* prog) {
  printf(
      "Usage: %s [options]\n"
      "\n"
      "  -c, --config PATH        config file (default %s, optional)\n"
      "  -u, --service-uuid UUID  128-bit service UUID; must end in 00. The\n"
      "                           characteristics use ...01, ...02, ...03\n"
      "  -n, --name NAME          advertised Bluetooth name\n"
      "  -N, --name-file PATH     read the advertised name from a file\n"
      "                           (first line, trimmed)\n"
      "  -V, --version            print the version and exit\n"
      "  -h, --help               show this help\n"
      "\n"
      "Config file keys (key = value, '#' comments):\n"
      "  service_uuid, name, name_file\n"
      "Command-line flags override the config file. Without a name, the device\n"
      "advertises as rpi-prov-<last 4 of serial>.\n",
      prog, kDefaultConfigPath);
}

std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string unquote(const std::string& s) {
  if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front())
    return s.substr(1, s.size() - 2);
  return s;
}

// Canonical 8-4-4-4-12 hex form, lowercased, with "00" in the last position.
bool normalize_service_uuid(const std::string& in, std::string& out) {
  if (in.size() != 36) return false;
  std::string u;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return false;
    } else if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    u += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (u.compare(34, 2, "00") != 0) return false;
  out = u;
  return true;
}

// The name source picked by one layer (config file or CLI). Exactly one of the
// two may be set per layer; a later layer replaces an earlier one entirely.
struct NameSource {
  std::string name;
  std::string file;
  bool empty() const { return name.empty() && file.empty(); }
};

bool read_name_file(const std::string& path, std::string& out) {
  std::ifstream f(path);
  if (!f) {
    logging::error("cannot read name file %s", path.c_str());
    return false;
  }
  std::string line;
  std::getline(f, line);
  out = trim(line);
  if (out.empty()) {
    logging::error("name file %s is empty", path.c_str());
    return false;
  }
  return true;
}

bool validate_name(const std::string& name) {
  for (unsigned char c : name) {
    if (c < 0x20 || c == 0x7f) {
      logging::error("device name contains control characters");
      return false;
    }
  }
  if (name.size() > kMaxAdvNameBytes) {
    logging::warn("device name is %zu bytes; BLE advertising fits %zu, so it "
                  "may be truncated",
                  name.size(), kMaxAdvNameBytes);
  }
  return true;
}

// Load key=value pairs from `path` into uuid/name. Returns false on a read or
// syntax error.
bool load_file(const std::string& path, std::string& uuid, NameSource& name) {
  std::ifstream f(path);
  if (!f) {
    logging::error("cannot read config file %s", path.c_str());
    return false;
  }
  std::string line;
  int lineno = 0;
  while (std::getline(f, line)) {
    ++lineno;
    std::string t = trim(line);
    if (t.empty() || t[0] == '#') continue;
    auto eq = t.find('=');
    if (eq == std::string::npos) {
      logging::error("%s:%d: expected key = value", path.c_str(), lineno);
      return false;
    }
    std::string key = trim(t.substr(0, eq));
    std::string val = unquote(trim(t.substr(eq + 1)));
    if (key == "service_uuid") {
      uuid = val;
    } else if (key == "name") {
      name.name = val;
    } else if (key == "name_file") {
      name.file = val;
    } else {
      logging::error("%s:%d: unknown key '%s'", path.c_str(), lineno, key.c_str());
      return false;
    }
  }
  if (!name.name.empty() && !name.file.empty()) {
    logging::error("%s: set either name or name_file, not both", path.c_str());
    return false;
  }
  return true;
}

bool file_exists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

} // namespace

ParseResult parse(int argc, char** argv, Config& out) {
  std::string config_path;
  std::string cli_uuid;
  NameSource cli_name;

  static const option kLongOpts[] = {
      {"config", required_argument, nullptr, 'c'},
      {"service-uuid", required_argument, nullptr, 'u'},
      {"name", required_argument, nullptr, 'n'},
      {"name-file", required_argument, nullptr, 'N'},
      {"version", no_argument, nullptr, 'V'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0},
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "c:u:n:N:Vh", kLongOpts, nullptr)) != -1) {
    switch (opt) {
      case 'c': config_path = optarg; break;
      case 'u': cli_uuid = optarg; break;
      case 'n': cli_name.name = optarg; break;
      case 'N': cli_name.file = optarg; break;
      case 'V': printf("rpi-ble-provision %s\n", kVersion); return ParseResult::Exit;
      case 'h': print_usage(argv[0]); return ParseResult::Exit;
      default: print_usage(argv[0]); return ParseResult::Error;
    }
  }
  if (optind < argc) {
    logging::error("unexpected argument '%s'", argv[optind]);
    return ParseResult::Error;
  }
  if (!cli_name.name.empty() && !cli_name.file.empty()) {
    logging::error("use either --name or --name-file, not both");
    return ParseResult::Error;
  }

  // An explicit --config must exist; the default path is optional.
  std::string uuid = kDefaultServiceUUID;
  NameSource name;
  if (config_path.empty() && file_exists(kDefaultConfigPath))
    config_path = kDefaultConfigPath;
  if (!config_path.empty()) {
    if (!load_file(config_path, uuid, name)) return ParseResult::Error;
    logging::info("loaded config %s", config_path.c_str());
  }
  if (!cli_uuid.empty()) uuid = cli_uuid;
  if (!cli_name.empty()) name = cli_name;

  if (!normalize_service_uuid(uuid, out.service_uuid)) {
    logging::error("invalid service UUID '%s': expected "
                   "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxx00",
                   uuid.c_str());
    return ParseResult::Error;
  }
  const std::string base = out.service_uuid.substr(0, 34);
  out.ssid_list_uuid = base + "01";
  out.creds_uuid = base + "02";
  out.status_uuid = base + "03";

  out.device_name.clear();
  if (!name.file.empty()) {
    if (!read_name_file(name.file, out.device_name)) return ParseResult::Error;
  } else {
    out.device_name = trim(name.name);
  }
  if (!out.device_name.empty() && !validate_name(out.device_name))
    return ParseResult::Error;

  return ParseResult::Ok;
}

} // namespace config
