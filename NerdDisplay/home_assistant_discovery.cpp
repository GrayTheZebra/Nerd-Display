#include "home_assistant_discovery.h"
#include "app.h"
#include "mqtt_topics.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

namespace {
  const char* EFFECTS[] = {
    "PRINT", "SLICE", "WIPE", "WIPE_CURSOR", "OPENING", "OPENING_CURSOR",
    "CLOSING", "CLOSING_CURSOR", "BLINDS", "DISSOLVE", "SCROLL_UP",
    "SCROLL_DOWN", "SCROLL_LEFT", "SCROLL_RIGHT", "SCROLL_UP_LEFT",
    "SCROLL_UP_RIGHT", "SCROLL_DOWN_LEFT", "SCROLL_DOWN_RIGHT", "SCAN_HORIZ",
    "SCAN_VERT", "GROW_UP", "GROW_DOWN"
  };

  String objectPrefix() {
    String id = App::deviceId;
    id.toLowerCase();
    return "nerd_display_" + id;
  }

  String discoveryTopic(const String& component, const String& objectId) {
    return "homeassistant/" + component + "/" + objectPrefix() + "/" + objectId + "/config";
  }

  void addDevice(JsonObject root) {
    JsonObject dev = root.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(objectPrefix());
    dev["name"] = "Nerd-Display " + App::deviceId;
    dev["manufacturer"] = "GrayTheZebra";
    dev["model"] = "Nerd-Display";
    dev["configuration_url"] = "http://" + WiFi.localIP().toString();
  }

  void addCommon(JsonObject root, const String& objectId, const String& name) {
    root["name"] = name;
    root["unique_id"] = objectPrefix() + "_" + objectId;
    addDevice(root);
  }

  void publishDoc(const String& topic, DynamicJsonDocument& doc) {
    String payload;
    serializeJson(doc, payload);
    App::mqtt.publish(topic.c_str(), payload.c_str(), true);
  }

  void publishNumber(const String& objectId, const String& name,
                     const String& key, long minValue, long maxValue,
                     long step, const char* mode = "box") {
    DynamicJsonDocument doc(1024);
    JsonObject root = doc.to<JsonObject>();
    addCommon(root, objectId, name);
    root["command_topic"] = Topics::set(key);
    root["state_topic"] = Topics::state(key);
    root["min"] = minValue;
    root["max"] = maxValue;
    root["step"] = step;
    root["mode"] = mode;
    publishDoc(discoveryTopic("number", objectId), doc);
  }

  void publishSelect(const String& objectId, const String& name, const String& key) {
    DynamicJsonDocument doc(1536);
    JsonObject root = doc.to<JsonObject>();
    addCommon(root, objectId, name);
    root["command_topic"] = Topics::set(key);
    root["state_topic"] = Topics::state(key);
    root["value_template"] = "{{ value_json }}";
    JsonArray options = root.createNestedArray("options");
    for (const char* effect : EFFECTS) options.add(effect);
    publishDoc(discoveryTopic("select", objectId), doc);
  }

  void publishText() {
    DynamicJsonDocument doc(1024);
    JsonObject root = doc.to<JsonObject>();
    addCommon(root, "text", "Text");
    root["command_topic"] = Topics::set("text");
    root["state_topic"] = Topics::state("text");
    root["value_template"] = "{{ value_json }}";
    publishDoc(discoveryTopic("text", "text"), doc);
  }

  void publishInfoSensor(const String& objectId, const String& name,
                         const String& jsonKey, const String& icon) {
    DynamicJsonDocument doc(1024);
    JsonObject root = doc.to<JsonObject>();
    addCommon(root, objectId, name);
    root["state_topic"] = App::baseTopic + "info";
    root["value_template"] = "{{ value_json." + jsonKey + " }}";
    root["entity_category"] = "diagnostic";
    root["icon"] = icon;
    publishDoc(discoveryTopic("sensor", objectId), doc);
  }

  void publishCurrentTextState() {
    const String text = App::params.messages.empty()
      ? String("Nerd-Display")
      : App::params.messages[0].text;

    DynamicJsonDocument doc(512);
    doc.set(text);
    String payload;
    serializeJson(doc, payload);
    App::mqtt.publish(Topics::state("text").c_str(), payload.c_str(), true);
  }
}

namespace HomeAssistantDiscovery {
  void publish() {
    if (!App::mqtt.connected()) return;

    publishText();
    publishNumber("brightness", "Helligkeit", "brightness", 0, 15, 1, "slider");
    publishNumber("speed", "Geschwindigkeit", "speed", 1, 65535, 1);
    publishNumber("dwell", "Anzeigedauer", "dwell", 0, 600000, 100);
    publishSelect("effect_in", "Einblendeffekt", "effect_in");
    publishSelect("effect_out", "Ausblendeffekt", "effect_out");

    publishInfoSensor("ip", "IP-Adresse", "ip", "mdi:ip-network");
    publishInfoSensor("mdns", "mDNS", "mdns", "mdi:dns");
    publishInfoSensor("mode", "Status", "mode", "mdi:information-outline");

    // Der bestehende MQTT-Code sendet Text-Acks als JSON-String. Damit der
    // Text-Sensor direkt nach dem Start einen Zustand besitzt, veröffentlichen
    // wir hier einmal den aktuellen Wert im gleichen Format.
    publishCurrentTextState();
  }
}
