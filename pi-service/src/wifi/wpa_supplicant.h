#pragma once
#include "wifi/backend.h"

namespace wifi {
// Factory for the wpa_supplicant (wpa_cli + iw) backend.
std::unique_ptr<Backend> make_wpa_supplicant();
} // namespace wifi
