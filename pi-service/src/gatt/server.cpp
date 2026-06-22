#include "gatt/server.h"

#include <fstream>
#include <thread>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>

#include "dbus/bluez.h"
#include "util/log.h"

namespace gatt {
namespace {

using json = nlohmann::json;

// D-Bus object paths for our GATT application, advertisement and agent.
constexpr const char* kApp = "/si/xpro/rpiprov";
constexpr const char* kSvc = "/si/xpro/rpiprov/service0";
constexpr const char* kCharSsid = "/si/xpro/rpiprov/service0/char0";
constexpr const char* kCharCreds = "/si/xpro/rpiprov/service0/char1";
constexpr const char* kCharStatus = "/si/xpro/rpiprov/service0/char2";
constexpr const char* kAdv = "/si/xpro/rpiprov/adv0";
constexpr const char* kAgent = "/si/xpro/rpiprov/agent0";

constexpr const char* kIfaceObjMgr = "org.freedesktop.DBus.ObjectManager";
constexpr const char* kIfaceService = "org.bluez.GattService1";
constexpr const char* kIfaceChar = "org.bluez.GattCharacteristic1";
constexpr const char* kIfaceAdv = "org.bluez.LEAdvertisement1";
constexpr const char* kIfaceAgent = "org.bluez.Agent1";

// Introspection XML, one interface per object type.
const char* kXmlObjMgr = R"XML(
<node><interface name="org.freedesktop.DBus.ObjectManager">
  <method name="GetManagedObjects">
    <arg name="objects" type="a{oa{sa{sv}}}" direction="out"/>
  </method>
</interface></node>)XML";

const char* kXmlService = R"XML(
<node><interface name="org.bluez.GattService1">
  <property name="UUID" type="s" access="read"/>
  <property name="Primary" type="b" access="read"/>
</interface></node>)XML";

const char* kXmlChar = R"XML(
<node><interface name="org.bluez.GattCharacteristic1">
  <method name="ReadValue">
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="value" type="ay" direction="out"/>
  </method>
  <method name="WriteValue">
    <arg name="value" type="ay" direction="in"/>
    <arg name="options" type="a{sv}" direction="in"/>
  </method>
  <method name="StartNotify"/>
  <method name="StopNotify"/>
  <property name="UUID" type="s" access="read"/>
  <property name="Service" type="o" access="read"/>
  <property name="Flags" type="as" access="read"/>
  <property name="Notifying" type="b" access="read"/>
</interface></node>)XML";

const char* kXmlAdv = R"XML(
<node><interface name="org.bluez.LEAdvertisement1">
  <method name="Release"/>
  <property name="Type" type="s" access="read"/>
  <property name="ServiceUUIDs" type="as" access="read"/>
  <property name="LocalName" type="s" access="read"/>
  <property name="Discoverable" type="b" access="read"/>
</interface></node>)XML";

const char* kXmlAgent = R"XML(
<node><interface name="org.bluez.Agent1">
  <method name="Release"/>
  <method name="RequestPinCode"><arg type="o" direction="in"/><arg type="s" direction="out"/></method>
  <method name="DisplayPinCode"><arg type="o" direction="in"/><arg type="s" direction="in"/></method>
  <method name="RequestPasskey"><arg type="o" direction="in"/><arg type="u" direction="out"/></method>
  <method name="DisplayPasskey"><arg type="o" direction="in"/><arg type="u" direction="in"/><arg type="q" direction="in"/></method>
  <method name="RequestConfirmation"><arg type="o" direction="in"/><arg type="u" direction="in"/></method>
  <method name="RequestAuthorization"><arg type="o" direction="in"/></method>
  <method name="AuthorizeService"><arg type="o" direction="in"/><arg type="s" direction="in"/></method>
  <method name="Cancel"/>
</interface></node>)XML";

const char* uuid_for_path(const char* path) {
  if (g_strcmp0(path, kCharSsid) == 0) return kSsidListUUID;
  if (g_strcmp0(path, kCharCreds) == 0) return kCredsUUID;
  if (g_strcmp0(path, kCharStatus) == 0) return kStatusUUID;
  return "";
}

std::vector<const char*> flags_for_path(const char* path) {
  if (g_strcmp0(path, kCharSsid) == 0) return {"encrypt-read", "encrypt-notify"};
  if (g_strcmp0(path, kCharCreds) == 0) return {"encrypt-write"};
  if (g_strcmp0(path, kCharStatus) == 0) return {"encrypt-read", "encrypt-notify"};
  return {};
}

// Build a floating a{sv} from name/value pairs (values may be floating).
GVariant* props_dict(std::initializer_list<std::pair<const char*, GVariant*>> items) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  for (const auto& it : items) g_variant_builder_add(&b, "{sv}", it.first, it.second);
  return g_variant_builder_end(&b);
}

GVariant* string_array(const std::vector<const char*>& items) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("as"));
  for (const char* s : items) g_variant_builder_add(&b, "s", s);
  return g_variant_builder_end(&b);
}

// Build a single-interface a{sa{sv}} dict: {iface: props}.
GVariant* one_iface(const char* iface, GVariant* props) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sa{sv}}"));
  g_variant_builder_add(&b, "{s@a{sv}}", iface, props);
  return g_variant_builder_end(&b);
}

GVariant* char_props(const char* path) {
  return props_dict({
      {"UUID", g_variant_new_string(uuid_for_path(path))},
      {"Service", g_variant_new_object_path(kSvc)},
      {"Flags", string_array(flags_for_path(path))},
      {"Notifying", g_variant_new_boolean(FALSE)},
  });
}

// Wrap a byte payload as a "(ay)" reply tuple.
GVariant* ay_reply(const std::string& s) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("ay"));
  for (unsigned char c : s) g_variant_builder_add(&b, "y", c);
  return g_variant_new("(@ay)", g_variant_builder_end(&b));
}

// Extract the "offset" from a ReadValue options dict ("(a{sv})"). Values larger
// than one MTU are read with a sequence of offsets (BLE "read long"), so we must
// honor it and return the tail of the value from there.
guint16 read_offset(GVariant* params) {
  guint16 offset = 0;
  GVariant* opts = nullptr;
  g_variant_get(params, "(@a{sv})", &opts);
  if (opts) {
    GVariant* ov = g_variant_lookup_value(opts, "offset", G_VARIANT_TYPE_UINT16);
    if (ov) {
      offset = g_variant_get_uint16(ov);
      g_variant_unref(ov);
    }
    g_variant_unref(opts);
  }
  return offset;
}

std::string from_offset(const std::string& s, guint16 offset) {
  return offset >= s.size() ? std::string() : s.substr(offset);
}

// Reject SSID/PSK containing control characters (newlines, NUL, etc.) so they
// cannot inject extra directives into wpa_supplicant's line-based config.
bool clean_field(const std::string& s) {
  for (unsigned char c : s)
    if (c < 0x20) return false;
  return true;
}

std::string status_to_json(const wifi::Status& s) {
  json j;
  j["state"] = s.state;
  j["ip"] = s.ip;
  j["ssid"] = s.ssid;
  j["error"] = s.error;
  return j.dump();
}

std::string failed_status(const std::string& ssid, const std::string& error) {
  return status_to_json({"failed", "", ssid, error});
}

// Best-effort stable suffix for the advertised name: last 4 of the Pi serial,
// else the hostname.
std::string name_suffix() {
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("Serial", 0) == 0) {
      auto colon = line.find(':');
      if (colon != std::string::npos) {
        std::string serial = line.substr(colon + 1);
        // trim
        while (!serial.empty() && (serial.front() == ' ' || serial.front() == '\t'))
          serial.erase(serial.begin());
        if (serial.size() >= 4) return serial.substr(serial.size() - 4);
      }
    }
  }
  char host[256] = {0};
  if (gethostname(host, sizeof(host) - 1) == 0 && host[0]) return host;
  return "0000";
}

// vtable trampolines -> Server methods.
void on_method(GDBusConnection*, const gchar*, const gchar* object_path,
               const gchar* interface_name, const gchar* method_name,
               GVariant* parameters, GDBusMethodInvocation* invocation,
               gpointer user_data) {
  static_cast<Server*>(user_data)->handle_method(object_path, interface_name,
                                                 method_name, parameters,
                                                 invocation);
}

GVariant* on_get_property(GDBusConnection*, const gchar*, const gchar* object_path,
                          const gchar* interface_name, const gchar* property_name,
                          GError** error, gpointer user_data) {
  GVariant* v = static_cast<Server*>(user_data)->handle_get_property(
      object_path, interface_name, property_name);
  if (!v) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                "unknown property %s", property_name);
  }
  return v;
}

const GDBusInterfaceVTable kVTable = {on_method, on_get_property, nullptr,
                                      {nullptr}};

// Kept alive for the process lifetime; freeing requires no teardown ordering.
std::vector<GDBusNodeInfo*> g_nodes;

GDBusInterfaceInfo* iface_info(const char* xml) {
  GError* err = nullptr;
  GDBusNodeInfo* node = g_dbus_node_info_new_for_xml(xml, &err);
  if (!node) {
    logging::error("introspection parse failed: %s", err ? err->message : "?");
    if (err) g_error_free(err);
    return nullptr;
  }
  g_nodes.push_back(node);
  return node->interfaces[0];
}

} // namespace

Server::Server(std::unique_ptr<wifi::Backend> wifi)
    : wifi_(std::move(wifi)) {}

Server::~Server() {
  if (conn_) {
    for (guint id : reg_ids_) g_dbus_connection_unregister_object(conn_, id);
    g_object_unref(conn_);
  }
}

bool Server::start() {
  GError* err = nullptr;
  conn_ = bluez::connect_system(&err);
  if (!conn_) {
    logging::error("system bus connect failed: %s", err ? err->message : "?");
    if (err) g_error_free(err);
    return false;
  }
  adapter_ = bluez::find_adapter(conn_);
  if (adapter_.empty()) {
    logging::error("no Bluetooth adapter found");
    return false;
  }
  local_name_ = "rpi-prov-" + name_suffix();
  logging::info("adapter %s, advertising as '%s'", adapter_.c_str(), local_name_.c_str());

  prepare_adapter();
  if (!register_objects()) return false;

  {
    std::lock_guard<std::mutex> lk(mtx_);
    status_payload_ = status_to_json(wifi_->status());
  }

  if (!register_with_bluez()) return false;
  logging::info("provisioning service ready");
  return true;
}

void Server::prepare_adapter() {
  bluez::set_property(conn_, adapter_, "org.bluez.Adapter1", "Powered",
                     g_variant_new_boolean(TRUE));
  bluez::set_property(conn_, adapter_, "org.bluez.Adapter1", "Pairable",
                     g_variant_new_boolean(TRUE));
  bluez::set_property(conn_, adapter_, "org.bluez.Adapter1", "PairableTimeout",
                     g_variant_new_uint32(0));
  bluez::set_property(conn_, adapter_, "org.bluez.Adapter1", "Discoverable",
                     g_variant_new_boolean(TRUE));
  bluez::set_property(conn_, adapter_, "org.bluez.Adapter1", "DiscoverableTimeout",
                     g_variant_new_uint32(0));
}

bool Server::register_objects() {
  GDBusInterfaceInfo* objmgr = iface_info(kXmlObjMgr);
  GDBusInterfaceInfo* service = iface_info(kXmlService);
  GDBusInterfaceInfo* characteristic = iface_info(kXmlChar);
  GDBusInterfaceInfo* adv = iface_info(kXmlAdv);
  GDBusInterfaceInfo* agent = iface_info(kXmlAgent);
  if (!objmgr || !service || !characteristic || !adv || !agent) return false;

  struct Reg { const char* path; GDBusInterfaceInfo* info; };
  const Reg regs[] = {
      {kApp, objmgr},        {kSvc, service},
      {kCharSsid, characteristic}, {kCharCreds, characteristic},
      {kCharStatus, characteristic}, {kAdv, adv},
      {kAgent, agent},
  };
  for (const auto& r : regs) {
    GError* err = nullptr;
    guint id = g_dbus_connection_register_object(conn_, r.path, r.info, &kVTable,
                                                 this, nullptr, &err);
    if (id == 0) {
      logging::error("register_object %s failed: %s", r.path,
                 err ? err->message : "?");
      if (err) g_error_free(err);
      return false;
    }
    reg_ids_.push_back(id);
  }
  return true;
}

bool Server::register_with_bluez() {
  // These are fired asynchronously: BlueZ services RegisterApplication /
  // RegisterAdvertisement by calling back into our objects (GetManagedObjects /
  // GetAll), which only works while the main loop is running. A synchronous
  // call here — before g_main_loop_run — would deadlock. Outcomes are logged by
  // the async callbacks.
  bluez::call_async(conn_, "/org/bluez", "org.bluez.AgentManager1",
                    "RegisterAgent",
                    g_variant_new("(os)", kAgent, "NoInputNoOutput"),
                    "RegisterAgent");
  bluez::call_async(conn_, "/org/bluez", "org.bluez.AgentManager1",
                    "RequestDefaultAgent", g_variant_new("(o)", kAgent),
                    "RequestDefaultAgent");
  bluez::call_async(conn_, adapter_, "org.bluez.GattManager1",
                    "RegisterApplication",
                    g_variant_new("(oa{sv})", kApp, nullptr),
                    "RegisterApplication");
  bluez::call_async(conn_, adapter_, "org.bluez.LEAdvertisingManager1",
                    "RegisterAdvertisement",
                    g_variant_new("(oa{sv})", kAdv, nullptr),
                    "RegisterAdvertisement");
  return true;
}

GVariant* Server::managed_objects() {
  GVariantBuilder objs;
  g_variant_builder_init(&objs, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
  g_variant_builder_add(
      &objs, "{o@a{sa{sv}}}", kSvc,
      one_iface(kIfaceService,
                props_dict({{"UUID", g_variant_new_string(kServiceUUID)},
                            {"Primary", g_variant_new_boolean(TRUE)}})));
  for (const char* path : {kCharSsid, kCharCreds, kCharStatus}) {
    g_variant_builder_add(&objs, "{o@a{sa{sv}}}", path,
                          one_iface(kIfaceChar, char_props(path)));
  }
  return g_variant_new("(a{oa{sa{sv}}})", &objs);
}

GVariant* Server::handle_get_property(const char* object_path, const char* iface,
                                      const char* property) {
  if (g_strcmp0(iface, kIfaceService) == 0) {
    if (g_strcmp0(property, "UUID") == 0) return g_variant_new_string(kServiceUUID);
    if (g_strcmp0(property, "Primary") == 0) return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(iface, kIfaceChar) == 0) {
    if (g_strcmp0(property, "UUID") == 0)
      return g_variant_new_string(uuid_for_path(object_path));
    if (g_strcmp0(property, "Service") == 0)
      return g_variant_new_object_path(kSvc);
    if (g_strcmp0(property, "Flags") == 0)
      return string_array(flags_for_path(object_path));
    if (g_strcmp0(property, "Notifying") == 0) {
      bool n = (g_strcmp0(object_path, kCharStatus) == 0) ? status_notifying_
                                                          : ssid_notifying_;
      return g_variant_new_boolean(n);
    }
  } else if (g_strcmp0(iface, kIfaceAdv) == 0) {
    if (g_strcmp0(property, "Type") == 0) return g_variant_new_string("peripheral");
    if (g_strcmp0(property, "ServiceUUIDs") == 0)
      return string_array({kServiceUUID});
    if (g_strcmp0(property, "LocalName") == 0)
      return g_variant_new_string(local_name_.c_str());
    if (g_strcmp0(property, "Discoverable") == 0)
      return g_variant_new_boolean(TRUE);
  }
  return nullptr;
}

void Server::handle_method(const char* object_path, const char* iface,
                           const char* method, GVariant* params,
                           GDBusMethodInvocation* inv) {
  // ObjectManager
  if (g_strcmp0(iface, kIfaceObjMgr) == 0 &&
      g_strcmp0(method, "GetManagedObjects") == 0) {
    g_dbus_method_invocation_return_value(inv, managed_objects());
    return;
  }

  // GATT characteristic
  if (g_strcmp0(iface, kIfaceChar) == 0) {
    if (g_strcmp0(method, "ReadValue") == 0) {
      const guint16 offset = read_offset(params);
      if (g_strcmp0(object_path, kCharSsid) == 0) {
        // A read reliably reaches us even when iOS reuses a cached subscription
        // and skips re-issuing StartNotify, so we (re)trigger the scan+stream
        // here. The read itself returns the previous snapshot immediately; the
        // fresh list arrives via notifications.
        if (offset == 0) start_ssid_stream();
        std::string js;
        {
          std::lock_guard<std::mutex> lk(mtx_);
          js = ssid_cache_;
        }
        g_dbus_method_invocation_return_value(inv, ay_reply(from_offset(js, offset)));
      } else if (g_strcmp0(object_path, kCharStatus) == 0) {
        g_dbus_method_invocation_return_value(
            inv, ay_reply(from_offset(status_json_locked(), offset)));
      } else {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.bluez.Error.NotSupported", "characteristic not readable");
      }
      return;
    }
    if (g_strcmp0(method, "WriteValue") == 0) {
      GVariant* ayv = nullptr;
      g_variant_get(params, "(@aya{sv})", &ayv, nullptr);
      gsize n = 0;
      gconstpointer data = g_variant_get_fixed_array(ayv, &n, 1);
      std::string payload(static_cast<const char*>(data), n);
      g_variant_unref(ayv);
      if (g_strcmp0(object_path, kCharCreds) == 0) {
        on_credentials(payload);
        g_dbus_method_invocation_return_value(inv, nullptr);
      } else {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.bluez.Error.NotSupported", "characteristic not writable");
      }
      return;
    }
    if (g_strcmp0(method, "StartNotify") == 0 ||
        g_strcmp0(method, "StopNotify") == 0) {
      bool on = g_strcmp0(method, "StartNotify") == 0;
      const char* which = (g_strcmp0(object_path, kCharStatus) == 0) ? "status"
                          : (g_strcmp0(object_path, kCharSsid) == 0) ? "ssid"
                                                                     : "?";
      if (g_strcmp0(object_path, kCharStatus) == 0) status_notifying_ = on;
      else if (g_strcmp0(object_path, kCharSsid) == 0) ssid_notifying_ = on;
      logging::info("%s on %s characteristic", method, which);
      g_dbus_method_invocation_return_value(inv, nullptr);
      // iOS does not perform a GATT Read Long, so the (multi-MTU) SSID list
      // cannot be delivered in one read. Instead, when the client subscribes we
      // stream the list as notifications: one network per notification followed
      // by a {"done":true} sentinel.
      if (on && g_strcmp0(object_path, kCharSsid) == 0) start_ssid_stream();
      return;
    }
  }

  // Advertisement.Release / Agent.* — accept everything (Just Works).
  if (g_strcmp0(iface, kIfaceAdv) == 0 || g_strcmp0(iface, kIfaceAgent) == 0) {
    if (g_strcmp0(method, "RequestPinCode") == 0 ||
        g_strcmp0(method, "RequestPasskey") == 0) {
      // Not supported with NoInputNoOutput.
      g_dbus_method_invocation_return_dbus_error(
          inv, "org.bluez.Error.Rejected", "no input/output");
      return;
    }
    g_dbus_method_invocation_return_value(inv, nullptr);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(inv, "org.bluez.Error.NotSupported",
                                             "unknown method");
}

void Server::start_ssid_stream() {
  // Guard against overlapping streams (StartNotify and the read-trigger can both
  // fire). Released after the notifications are emitted.
  if (ssid_stream_active_.exchange(true)) return;
  std::thread([this]() {
    // Scan on the worker thread (blocking), then build one small JSON frame per
    // network plus a sentinel. Each frame is well under any MTU.
    auto nets = wifi_->scan();
    json arr = json::array();
    auto* frames = new std::vector<std::string>();
    for (const auto& n : nets) {
      json j = {{"ssid", n.ssid}, {"signal", n.signal}, {"secured", n.secured}};
      frames->push_back(j.dump());
      arr.push_back(j);
    }
    frames->push_back("{\"done\":true}");
    {
      // Keep a snapshot so a plain ReadValue has something to return too.
      std::lock_guard<std::mutex> lk(mtx_);
      ssid_cache_ = arr.dump();
    }
    logging::info("streaming %zu networks to subscriber", nets.size());

    // Emit the notifications on the main loop (GDBus dispatch thread).
    auto* ctx = new std::pair<Server*, std::vector<std::string>*>(this, frames);
    g_idle_add(
        [](gpointer ud) -> gboolean {
          auto* p = static_cast<std::pair<Server*, std::vector<std::string>*>*>(ud);
          for (const auto& f : *p->second) p->first->emit_value(kCharSsid, f);
          p->first->ssid_stream_active_.store(false);
          delete p->second;
          delete p;
          return G_SOURCE_REMOVE;
        },
        ctx);
  }).detach();
}

std::string Server::status_json_locked() {
  if (connect_in_progress_.load()) {
    std::lock_guard<std::mutex> lk(mtx_);
    return status_payload_;
  }
  std::string js = status_to_json(wifi_->status());
  std::lock_guard<std::mutex> lk(mtx_);
  status_payload_ = js;
  return js;
}

void Server::on_credentials(const std::string& payload) {
  std::string ssid, psk;
  try {
    json j = json::parse(payload);
    ssid = j.at("ssid").get<std::string>();
    psk = j.value("psk", std::string());
  } catch (const std::exception& e) {
    logging::warn("bad credentials payload: %s", e.what());
    std::string js = failed_status("", "invalid JSON");
    { std::lock_guard<std::mutex> lk(mtx_); status_payload_ = js; }
    publish_status(js);
    return;
  }

  if (ssid.empty() || !clean_field(ssid) || !clean_field(psk)) {
    std::string js = failed_status(ssid, "invalid SSID or password");
    { std::lock_guard<std::mutex> lk(mtx_); status_payload_ = js; }
    publish_status(js);
    return;
  }
  if (connect_in_progress_.exchange(true)) {
    logging::warn("connect already in progress; ignoring write");
    return;
  }
  start_connect(ssid, psk);
}

void Server::start_connect(std::string ssid, std::string psk) {
  logging::info("provisioning request for SSID '%s'", ssid.c_str());
  std::string connecting = status_to_json({"connecting", "", ssid, ""});
  { std::lock_guard<std::mutex> lk(mtx_); status_payload_ = connecting; }
  publish_status(connecting);

  std::thread([this, ssid, psk]() {
    std::string err;
    bool ok = wifi_->connect(ssid, psk, err);
    std::string js = ok ? status_to_json(wifi_->status()) : failed_status(ssid, err);
    logging::info("provisioning result: %s", ok ? "connected" : err.c_str());
    { std::lock_guard<std::mutex> lk(mtx_); status_payload_ = js; }
    connect_in_progress_.store(false);
    g_idle_add(
        [](gpointer ud) -> gboolean {
          auto* self = static_cast<Server*>(ud);
          std::string payload;
          { std::lock_guard<std::mutex> lk(self->mtx_); payload = self->status_payload_; }
          self->emit_value(kCharStatus, payload);
          return G_SOURCE_REMOVE;
        },
        this);
  }).detach();
}

void Server::publish_status(const std::string& json_str) {
  emit_value(kCharStatus, json_str);
}

void Server::emit_value(const char* object_path, const std::string& value) {
  bool notifying = (g_strcmp0(object_path, kCharStatus) == 0) ? status_notifying_
                                                              : ssid_notifying_;
  if (!notifying) return;

  GVariantBuilder ay;
  g_variant_builder_init(&ay, G_VARIANT_TYPE("ay"));
  for (unsigned char c : value) g_variant_builder_add(&ay, "y", c);

  GVariantBuilder props;
  g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&props, "{sv}", "Value", g_variant_builder_end(&ay));

  GVariantBuilder inval;
  g_variant_builder_init(&inval, G_VARIANT_TYPE("as"));

  GError* err = nullptr;
  g_dbus_connection_emit_signal(
      conn_, nullptr, object_path, "org.freedesktop.DBus.Properties",
      "PropertiesChanged",
      g_variant_new("(sa{sv}as)", kIfaceChar, &props, &inval), &err);
  if (err) {
    logging::warn("emit PropertiesChanged failed: %s", err->message);
    g_error_free(err);
  }
}

} // namespace gatt
