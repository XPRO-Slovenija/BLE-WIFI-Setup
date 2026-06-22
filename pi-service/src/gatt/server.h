#pragma once
#include <gio/gio.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "wifi/backend.h"

namespace gatt {

// 128-bit UUIDs for the provisioning GATT service and its characteristics.
// These are the contract shared with the mobile app — keep them in sync.
constexpr const char* kServiceUUID = "7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e00";
constexpr const char* kSsidListUUID = "7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e01";
constexpr const char* kCredsUUID = "7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e02";
constexpr const char* kStatusUUID = "7a8b4e10-9c2d-4f3a-bf21-0a1b2c3d4e03";

// BlueZ GATT peripheral exposing the provisioning service. Talks to bluetoothd
// over the system D-Bus, registers a GATT application + advertisement + pairing
// agent, and applies received WiFi credentials through the wifi::Backend.
//
// Threading: D-Bus callbacks run on the GLib main loop thread. The (slow) WiFi
// join runs on a detached worker thread and marshals its result back to the
// main loop via g_idle_add before emitting notifications.
class Server {
 public:
  explicit Server(std::unique_ptr<wifi::Backend> wifi);
  ~Server();

  // Connect to the bus, prepare the adapter, and register all objects.
  bool start();

  // --- called from D-Bus vtable trampolines (public so free functions reach
  //     them; not part of the external API) ---
  GVariant* handle_get_property(const char* object_path, const char* iface,
                                const char* property);
  void handle_method(const char* object_path, const char* iface,
                     const char* method, GVariant* params,
                     GDBusMethodInvocation* inv);
  GVariant* managed_objects();

 private:
  bool register_objects();
  bool register_with_bluez();
  void prepare_adapter();

  // Characteristic value producers.
  std::string status_json_locked();

  // Stream the SSID list to a subscribed client as one-network-per-notification
  // (plus a {"done":true} sentinel), so it isn't limited by a single read's MTU.
  void start_ssid_stream();

  // Credential write -> background connect.
  void on_credentials(const std::string& payload);
  void start_connect(std::string ssid, std::string psk);
  void publish_status(const std::string& json); // emit notify on status char

  void emit_value(const char* object_path, const std::string& value);

  std::unique_ptr<wifi::Backend> wifi_;
  GDBusConnection* conn_ = nullptr;
  std::string adapter_;
  std::string local_name_;
  std::vector<guint> reg_ids_;

  std::mutex mtx_;
  std::string status_payload_;     // latest status JSON (guarded by mtx_)
  std::string ssid_cache_;         // snapshot for multi-MTU reads (guarded by mtx_)
  bool ssid_notifying_ = false;
  bool status_notifying_ = false;
  std::atomic<bool> connect_in_progress_{false};
  std::atomic<bool> ssid_stream_active_{false};
};

} // namespace gatt
