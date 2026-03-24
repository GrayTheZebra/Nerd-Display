#include "config_store.h"
#include "app.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

namespace ConfigStore {
  const char* CONF_PATH = "/config.json";

  String macSuffix() {
    uint8_t mac[6]; WiFi.macAddress(mac);
    char buf[7]; sprintf(buf, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(buf);
  }
  String idFromMAC() { return macSuffix(); }

  String normalizedBase(String in) {
    in.trim();
    while (in.indexOf("//") >= 0) in.replace("//", "/");
    while (in.startsWith("/")) in.remove(0, 1);
    while (in.endsWith("/"))   in.remove(in.length() - 1);
    return in;
  }

  String makeBaseTopic() {
    String t = normalizedBase(App::cfg.mqttBase);
    return t.length() ? (t + "/") : String();
  }

  void ensurePersistentDefaults() {
    if (App::cfg.mqttBase.length() == 0) {
      App::cfg.mqttBase = "zebra/nerd-display/" + idFromMAC();
    }
    if (App::cfg.mdnsName.length() == 0) {
      String mac = WiFi.macAddress(); mac.replace(":", "");
      App::cfg.mdnsName = mac.substring(mac.length() - 4);
      App::cfg.mdnsName.toLowerCase();
    }
  }

  void save() {
    DynamicJsonDocument doc(2048);
    doc["mqttHost"] = App::cfg.mqttHost;
    doc["mqttPort"] = App::cfg.mqttPort;
    doc["mqttUser"] = App::cfg.mqttUser;
    doc["mqttPass"] = App::cfg.mqttPass;
    doc["mqttBase"] = normalizedBase(App::cfg.mqttBase);
    doc["mdnsName"] = App::cfg.mdnsName;
    doc["displayCount"] = App::sanitizedDisplayCount(App::cfg.displayCount);

    File f = LittleFS.open(CONF_PATH, "w");
    if (!f) return;
    serializeJsonPretty(doc, f);
    f.close();
  }

  void load() {
    if (!LittleFS.exists(CONF_PATH)) return;
    File f = LittleFS.open(CONF_PATH, "r");
    if (!f) return;

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
      App::cfg.mqttHost = doc["mqttHost"] | App::cfg.mqttHost;
      App::cfg.mqttPort = doc["mqttPort"] | App::cfg.mqttPort;
      App::cfg.mqttUser = doc["mqttUser"] | App::cfg.mqttUser;
      App::cfg.mqttPass = doc["mqttPass"] | App::cfg.mqttPass;
      App::cfg.mqttBase = normalizedBase((const char*)(doc["mqttBase"] | App::cfg.mqttBase.c_str()));
      App::cfg.mdnsName = doc["mdnsName"] | App::cfg.mdnsName;
      App::cfg.displayCount = App::sanitizedDisplayCount((uint8_t)(doc["displayCount"] | App::cfg.displayCount));
    }
    f.close();
  }
}
