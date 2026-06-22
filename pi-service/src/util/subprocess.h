#pragma once
#include <string>
#include <vector>

namespace util {

struct ProcResult {
  int exit_code = -1;   // process exit status, or -1 if it could not be run / was signalled
  std::string out;      // captured stdout (stderr is captured separately)
  std::string err;      // captured stderr
};

// Run a program with an explicit argv vector. argv[0] is the program; it is
// resolved via PATH. The child is exec'd directly (NO shell), so arguments are
// passed verbatim and shell metacharacters in user input (SSID/PSK) cannot be
// interpreted as commands. This is the project's only sanctioned way to invoke
// external tools (nmcli, wpa_cli, iw, systemctl).
ProcResult run(const std::vector<std::string>& argv);

} // namespace util
