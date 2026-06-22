#pragma once
#include "wifi/backend.h"

namespace wifi {
// Factory for the NetworkManager (nmcli) backend.
std::unique_ptr<Backend> make_networkmanager();
} // namespace wifi
