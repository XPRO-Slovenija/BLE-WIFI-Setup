cmake_minimum_required(VERSION 3.16)
project(rpi_ble_provision CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

find_package(PkgConfig REQUIRED)
# GLib / GIO provide the GDBus + GMainLoop machinery we use to talk to BlueZ.
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0 glib-2.0 gobject-2.0)

# nlohmann/json: prefer the system package; fall back to fetching the single header.
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(rpi-ble-provision
  src/main.cpp
  src/util/subprocess.cpp
  src/wifi/networkmanager.cpp
  src/wifi/wpa_supplicant.cpp
  src/wifi/detect.cpp
  src/gatt/server.cpp
  src/dbus/bluez.cpp
)

target_include_directories(rpi-ble-provision PRIVATE src)
target_link_libraries(rpi-ble-provision PRIVATE PkgConfig::GIO nlohmann_json::nlohmann_json)

include(GNUInstallDirs)
install(TARGETS rpi-ble-provision RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
