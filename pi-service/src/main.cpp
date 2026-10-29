#include <glib-unix.h>

#include "config.h"
#include "gatt/server.h"
#include "util/log.h"
#include "wifi/backend.h"

namespace {
gboolean on_signal(gpointer user_data) {
  logging::info("signal received, shutting down");
  g_main_loop_quit(static_cast<GMainLoop*>(user_data));
  return G_SOURCE_REMOVE;
}
} // namespace

int main(int argc, char** argv) {
  config::Config cfg;
  switch (config::parse(argc, argv, cfg)) {
    case config::ParseResult::Ok: break;
    case config::ParseResult::Exit: return 0;
    case config::ParseResult::Error: return 2;
  }

  logging::info("rpi-ble-provision %s", config::kVersion);

  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);

  auto backend = wifi::detect();
  if (!backend) {
    logging::error("could not determine WiFi backend; exiting");
    return 1;
  }
  logging::info("using WiFi backend: %s", backend->name());

  gatt::Server server(std::move(cfg), std::move(backend));
  if (!server.start()) {
    logging::error("failed to start GATT server; exiting");
    return 1;
  }

  g_unix_signal_add(SIGINT, on_signal, loop);
  g_unix_signal_add(SIGTERM, on_signal, loop);

  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  return 0;
}
