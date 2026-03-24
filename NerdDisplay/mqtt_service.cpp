#include "mqtt_service.h"
#include "app.h"
#include "mqtt_topics.h"
#include "display_service.h"
#include "config_store.h"
#include <ArduinoJson.h>

namespace {
  bool parseZoneRange(const String& rawIn, int16_t& outFrom, int16_t& outTo) {
    String raw = rawIn;
    raw.trim();
    if (!raw.length()) return false;

    raw.replace(" ", "");
    const int sep = raw.indexOf('-');
    if (sep < 0) {
      int v = raw.toInt();
      if (v <= 0) return false;
      outFrom = (int16_t)v;
      outTo   = (int16_t)v;
      return true;
    }

    String a = raw.substring(0, sep);
    String b = raw.substring(sep + 1);
    if (!a.length() || !b.length()) return false;
    int from = a.toInt();
    int to   = b.toInt();
    if (from <= 0 || to <= 0) return false;
    if (from > to) {
      int tmp = from;
      from = to;
      to = tmp;
    }
    outFrom = (int16_t)from;
    outTo   = (int16_t)to;
    return true;
  }

  // Helpers
  inline void publishSetKey(const String &key, const String &val)   { App::mqtt.publish(Topics::set(key).c_str(),   val.c_str(), false); }
  inline void publishSetKey(const String &k, int v)                  { publishSetKey(k, String(v)); }
  inline void publishSetKey(const String &k, uint32_t v)             { publishSetKey(k, String(v)); }

  inline void publishStateKey(const String &key, const String &val)  { App::mqtt.publish(Topics::state(key).c_str(), val.c_str(), true); }
  inline void publishStateKey(const String &k, int v)                { publishStateKey(k, String(v)); }
  inline void publishStateKey(const String &k, uint32_t v)           { publishStateKey(k, String(v)); }

  inline void publishStateKeyQuoted(const String& key, const String& val) {
    String out = "\"" + val + "\"";
    App::mqtt.publish(Topics::state(key).c_str(), out.c_str(), true);
  }
  inline void ackEchoStateStr(const String &k, const String &v) { publishStateKeyQuoted(k, v); }

  void mqttCallback(char* topic, byte* payload, unsigned int length) {
    const String t(topic);
    String raw; raw.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) raw += (char)payload[i];
    raw.trim();

    // meta/set_bootstrapped
    if (t.endsWith("/meta/set_bootstrapped")) { App::setBootstrapped = (raw == "1"); return; }
    // legacy has_init
    if (t.endsWith("/has_init"))              { App::hasInitFlag = (raw == "1");      return; }

    // brightness
    if (t.endsWith("/set/brightness")) {
      if (raw.length() == 0) return;
      const int v = constrain(raw.toInt(), 0, 15);
      App::params.brightness = (uint8_t)v;
      Display::applyParams();
      publishStateKey("brightness", v);
      return;
    }
    // speed
    if (t.endsWith("/set/speed")) {
      if (raw.length() == 0) return;
      long v = raw.toInt();
      if (v <= 0) v = App::params.speed;
      App::params.speed = (uint16_t)constrain(v, 1, 65535);
      publishStateKey("speed", (int)App::params.speed);
      return;
    }
    // dwell
    if (t.endsWith("/set/dwell")) {
      if (raw.length() == 0) return;
      long v = raw.toInt();
      if (v < 0) v = App::params.dwell;
      App::params.dwell = (uint32_t)v;
      publishStateKey("dwell", (uint32_t)App::params.dwell);
      return;
    }
    // effect (legacy)
    if (t.endsWith("/set/effect")) {
      if (raw.length() == 0) return;
      String u = raw; u.toUpperCase();
      // einfacher: wir setzen beide direkt auf denselben Namen
      App::params.effect_in  = u;
      App::params.effect_out = u;
      ackEchoStateStr("effect_in",  App::params.effect_in);
      ackEchoStateStr("effect_out", App::params.effect_out);
      ackEchoStateStr("effect",     App::params.effect_in);
      return;
    }
    if (t.endsWith("/set/effect_in")) {
      if (raw.length() == 0) return;
      String u = raw; u.toUpperCase();
      App::params.effect_in = u;
      ackEchoStateStr("effect_in", App::params.effect_in);
      ackEchoStateStr("effect",    App::params.effect_in);
      return;
    }
    if (t.endsWith("/set/effect_out")) {
      if (raw.length() == 0) return;
      String u = raw; u.toUpperCase();
      App::params.effect_out = u;
      ackEchoStateStr("effect_out", App::params.effect_out);
      return;
    }
    // --- TEXT: String | {"text":""} | ["a","b"] | {"messages":[...]} | [{"text":"", "in":"", "out":"", "dwell":1234}, ...]
    if (t.endsWith("/set/text")) {
      if (raw.length() == 0) return;

      App::params.messages.clear();

      DynamicJsonDocument doc(1536);
      DeserializationError err = deserializeJson(doc, raw);

      // text + in/out + dwell (ms, -1 => global)
      auto pushSingle = [&](const String& txt, const String& in, const String& out, int32_t dwellMs, int16_t zoneFrom, int16_t zoneTo) {
        String s = txt; s.trim();
        if (!s.length()) return;
        const int16_t minZone = 1;
        const int16_t maxZone = (int16_t)App::cfg.displayCount;
        if (zoneFrom > 0 && zoneTo > 0) {
          zoneFrom = constrain(zoneFrom, minZone, maxZone);
          zoneTo   = constrain(zoneTo,   minZone, maxZone);
          if (zoneFrom > zoneTo) {
            const int16_t tmp = zoneFrom;
            zoneFrom = zoneTo;
            zoneTo = tmp;
          }
        } else {
          zoneFrom = -1;
          zoneTo = -1;
        }
        App::params.messages.push_back( MessageItem{ s, in, out, dwellMs, zoneFrom, zoneTo } );
      };

      if (!err) {
        if (doc.is<JsonArray>()) {
          // Entweder Array<String> oder Array<Object>
          for (JsonVariant v : doc.as<JsonArray>()) {
            if (v.is<const char*>()) {
              pushSingle(String(v.as<const char*>()), "", "", -1, -1, -1);
            } else if (v.is<JsonObject>()) {
              String txt = v["text"] | "";
              String ein = v["in"]   | "";
              String eout= v["out"]  | "";
              int32_t d  = (int32_t)(v["dwell"] | -1);
              int16_t zf = -1, zt = -1;
              if (v["Bereich"].is<const char*>()) {
                parseZoneRange(String(v["Bereich"].as<const char*>()), zf, zt);
              } else if (v["bereich"].is<const char*>()) {
                parseZoneRange(String(v["bereich"].as<const char*>()), zf, zt);
              } else if (v["range"].is<const char*>()) {
                parseZoneRange(String(v["range"].as<const char*>()), zf, zt);
              } else if (v["zone"].is<const char*>()) {
                parseZoneRange(String(v["zone"].as<const char*>()), zf, zt);
              }
              pushSingle(txt, ein, eout, d, zf, zt);
            }
          }
        } else if (doc.is<JsonObject>()) {
          if (doc["messages"].is<JsonArray>()) {
            for (JsonVariant v : doc["messages"].as<JsonArray>()) {
              if (v.is<const char*>()) {
                pushSingle(String(v.as<const char*>()), "", "", -1, -1, -1);
              } else if (v.is<JsonObject>()) {
                String txt = v["text"] | "";
                String ein = v["in"]   | "";
                String eout= v["out"]  | "";
                int32_t d  = (int32_t)(v["dwell"] | -1);
                int16_t zf = -1, zt = -1;
                if (v["Bereich"].is<const char*>()) {
                  parseZoneRange(String(v["Bereich"].as<const char*>()), zf, zt);
                } else if (v["bereich"].is<const char*>()) {
                  parseZoneRange(String(v["bereich"].as<const char*>()), zf, zt);
                } else if (v["range"].is<const char*>()) {
                  parseZoneRange(String(v["range"].as<const char*>()), zf, zt);
                } else if (v["zone"].is<const char*>()) {
                  parseZoneRange(String(v["zone"].as<const char*>()), zf, zt);
                }
                pushSingle(txt, ein, eout, d, zf, zt);
              }
            }
          } else if (doc["text"].is<const char*>()) {
            String txt = doc["text"].as<const char*>();
            String ein = doc["in"]   | "";
            String eout= doc["out"]  | "";
            int32_t d  = (int32_t)(doc["dwell"] | -1);
            int16_t zf = -1, zt = -1;
            if (doc["Bereich"].is<const char*>()) {
              parseZoneRange(String(doc["Bereich"].as<const char*>()), zf, zt);
            } else if (doc["bereich"].is<const char*>()) {
              parseZoneRange(String(doc["bereich"].as<const char*>()), zf, zt);
            } else if (doc["range"].is<const char*>()) {
              parseZoneRange(String(doc["range"].as<const char*>()), zf, zt);
            } else if (doc["zone"].is<const char*>()) {
              parseZoneRange(String(doc["zone"].as<const char*>()), zf, zt);
            }
            pushSingle(txt, ein, eout, d, zf, zt);
          }
        }
      }

      // Fallback: Plain-String
      if (App::params.messages.empty()) {
        String s = raw;
        if (s.length() >= 2 && s.startsWith("\"") && s.endsWith("\"")) {
          s = s.substring(1, s.length() - 1);
        }
        if (!s.length()) s = "Nerd-Display";
        pushSingle(s, "", "", -1, -1, -1);
      }

      // Neustarten
      App::msgIndex = 0;
      Display::nextMessage();

      // State-Ack: wir spiegeln weiterhin nur den ersten Text
      if (!App::params.messages.empty()) {
        String first = App::params.messages[0].text;
        String out = "\"" + first + "\"";
        App::mqtt.publish((Topics::state("text")).c_str(), out.c_str(), true);
      }
      return;
    }

  }
}

namespace Mqtt {
  void publishSetDefaultsRetained() {
    ::publishSetKey("brightness", (int)App::params.brightness);
    ::publishSetKey("speed",      (int)App::params.speed);
    ::publishSetKey("dwell",      (uint32_t)App::params.dwell);
    ::publishSetKey("effect_in",  App::params.effect_in);
    ::publishSetKey("effect_out", App::params.effect_out);
    ::publishSetKey("effect",     App::params.effect_in); // legacy
    const String txt = App::params.messages.empty()
      ? String("Nerd-Display")
      : App::params.messages[0].text;
    ::publishSetKey("text", txt);
  }

  void publishFullStateSnapshot() {
    ::publishStateKey("brightness", (int)App::params.brightness);
    ::publishStateKey("speed",      (int)App::params.speed);
    ::publishStateKey("dwell",      (uint32_t)App::params.dwell);
    ::publishStateKeyQuoted("effect_in",  App::params.effect_in);
    ::publishStateKeyQuoted("effect_out", App::params.effect_out);
    ::publishStateKeyQuoted("effect",     App::params.effect_in); // legacy
    const String txt = App::params.messages.empty()
      ? String("Nerd-Display")
      : App::params.messages[0].text;
    ::publishSetKey("text", txt);
  }

  void publishInfo(const String& mode) {
    if (!App::mqtt.connected()) return;
    DynamicJsonDocument d(256);
    d["ip"]   = WiFi.localIP().toString();
    d["mdns"] = App::mdnsHost + ".local";
    d["mode"] = mode;
    String out; serializeJson(d, out);
    App::mqtt.publish((App::baseTopic + "info").c_str(), out.c_str(), true);
  }

  void begin() {
    App::mqtt.setBufferSize(4096);
    App::mqtt.setCallback(::mqttCallback);
  }

  bool reconnect() {
    if (App::cfg.mqttHost.length() == 0) return false;

    App::mqtt.setServer(App::cfg.mqttHost.c_str(), App::cfg.mqttPort);

    const String clientId = "NerdDisplay-" + App::deviceId;
    const bool ok = (App::cfg.mqttUser.length())
      ? App::mqtt.connect(clientId.c_str(), App::cfg.mqttUser.c_str(), App::cfg.mqttPass.c_str())
      : App::mqtt.connect(clientId.c_str());
    if (!ok) return false;

    // Bootstrap-Check
    App::setBootstrapped = false;
    const String metaTopic = Topics::meta("set_bootstrapped");
    App::mqtt.subscribe(metaTopic.c_str());
    unsigned long t0 = millis();
    while (millis() - t0 < 600) { App::mqtt.loop(); delay(10); }
    App::mqtt.unsubscribe(metaTopic.c_str());

    if (!App::setBootstrapped) {
      publishSetDefaultsRetained();
      App::mqtt.publish(metaTopic.c_str(), "1", true);
    }

    publishFullStateSnapshot();

    App::mqtt.subscribe((App::baseTopic + "set/#").c_str());

    publishInfo("ready");
    return true;
  }

  void loop() { App::mqtt.loop(); }
}
