#include "dbus/bluez.h"

#include "util/log.h"

namespace bluez {

GDBusConnection* connect_system(GError** err) {
  return g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, err);
}

std::string find_adapter(GDBusConnection* conn) {
  GError* err = nullptr;
  GVariant* res = g_dbus_connection_call_sync(
      conn, kService, "/", "org.freedesktop.DBus.ObjectManager",
      "GetManagedObjects", nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
  if (!res) {
    logging::error("GetManagedObjects failed: %s", err ? err->message : "?");
    if (err) g_error_free(err);
    return "";
  }

  std::string adapter;
  GVariant* objects = g_variant_get_child_value(res, 0); // a{oa{sa{sv}}}
  GVariantIter it;
  g_variant_iter_init(&it, objects);
  const gchar* path = nullptr;
  GVariant* ifaces = nullptr;
  while (g_variant_iter_loop(&it, "{&o@a{sa{sv}}}", &path, &ifaces)) {
    GVariant* a = g_variant_lookup_value(ifaces, "org.bluez.Adapter1",
                                         G_VARIANT_TYPE("a{sv}"));
    if (a) {
      if (adapter.empty()) adapter = path;
      g_variant_unref(a);
    }
  }
  g_variant_unref(objects);
  g_variant_unref(res);
  return adapter;
}

bool set_property(GDBusConnection* conn, const std::string& path,
                  const char* iface, const char* name, GVariant* value) {
  GError* err = nullptr;
  GVariant* r = g_dbus_connection_call_sync(
      conn, kService, path.c_str(), "org.freedesktop.DBus.Properties", "Set",
      g_variant_new("(ssv)", iface, name, value), nullptr,
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
  if (!r) {
    logging::warn("Set %s.%s failed: %s", iface, name, err ? err->message : "?");
    if (err) g_error_free(err);
    return false;
  }
  g_variant_unref(r);
  return true;
}

bool call(GDBusConnection* conn, const std::string& path, const char* iface,
          const char* method, GVariant* params, GError** err) {
  GVariant* r = g_dbus_connection_call_sync(conn, kService, path.c_str(), iface,
                                            method, params, nullptr,
                                            G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                                            err);
  if (!r) return false;
  g_variant_unref(r);
  return true;
}

namespace {
void on_call_done(GObject* src, GAsyncResult* res, gpointer user_data) {
  char* what = static_cast<char*>(user_data);
  GError* err = nullptr;
  GVariant* r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
  if (!r) {
    logging::error("%s failed: %s", what, err ? err->message : "?");
    if (err) g_error_free(err);
  } else {
    logging::info("%s ok", what);
    g_variant_unref(r);
  }
  g_free(what);
}
} // namespace

void call_async(GDBusConnection* conn, const std::string& path, const char* iface,
                const char* method, GVariant* params, const char* what) {
  g_dbus_connection_call(conn, kService, path.c_str(), iface, method, params,
                         nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                         on_call_done, g_strdup(what));
}

} // namespace bluez
