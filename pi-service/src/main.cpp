#include <glib-unix.h>

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

int main() {
  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);

  auto backend = wifi::detect();
  if (!backend) {
    logging::error("could not determine WiFi backend; exiting");
    return 1;
  }
  logging::info("using WiFi backend: %s", backend->name());

  gatt::Server server(std::move(backend));
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
