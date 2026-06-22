#pragma once
#include <gio/gio.h>

#include <string>

// Thin helpers over the BlueZ D-Bus API on the system bus. These wrap the
// boilerplate of g_dbus_connection_call_sync so the GATT server code reads
// clearly.
namespace bluez {

constexpr const char* kService = "org.bluez";

// Connect to the system bus (where bluetoothd lives).
GDBusConnection* connect_system(GError** err);

// Return the object path of the first BlueZ adapter (e.g. "/org/bluez/hci0"),
// or "" if none is present.
std::string find_adapter(GDBusConnection* conn);

// Set a property on an object via org.freedesktop.DBus.Properties.Set.
// `value` is consumed (may be floating).
bool set_property(GDBusConnection* conn, const std::string& path,
                  const char* iface, const char* name, GVariant* value);

// Call a BlueZ method and discard the reply. `params` is consumed (may be
// floating or NULL). Returns false and leaves *err set on failure.
bool call(GDBusConnection* conn, const std::string& path, const char* iface,
          const char* method, GVariant* params, GError** err);

// Fire a BlueZ method asynchronously, logging the outcome under label `what`.
// MUST be used for RegisterApplication/RegisterAdvertisement: BlueZ services
// those by calling back into our objects (GetManagedObjects / GetAll), which
// only works while our main loop is running — a synchronous call here would
// deadlock. `params` is consumed (may be floating or NULL).
void call_async(GDBusConnection* conn, const std::string& path, const char* iface,
                const char* method, GVariant* params, const char* what);

} // namespace bluez
