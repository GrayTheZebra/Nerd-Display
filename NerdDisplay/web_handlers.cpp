#include "web_handlers.h"
#include "app.h"
#include "config_store.h"
#include "mqtt_topics.h"
#include "mqtt_service.h"
#include "webui.h"        // liefert HTML_PAGE
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>

namespace {
  String renderPage() {
    String page = FPSTR(HTML_PAGE);
    page.replace("%%ID%%", App::deviceId);
    page.replace("%%BT%%", App::baseTopic);
    page.replace("%%IP%%", WiFi.localIP().toString());
    page.replace("%%MDNS%%", App::mdnsHost + ".local");

    page.replace("%%mqttHost%%", App::cfg.mqttHost);
    page.replace("%%mqttPort%%", String(App::cfg.mqttPort));
    page.replace("%%mqttUser%%", App::cfg.mqttUser);
    page.replace("%%mqttPass%%", App::cfg.mqttPass);
    page.replace("%%mqttBase%%", App::cfg.mqttBase);
    page.replace("%%mdnsName%%", App::cfg.mdnsName);
    page.replace("%%displayCount%%", String(App::cfg.displayCount));
    page.replace("%%displayMin%%", String(App::MIN_DEVICES));
    page.replace("%%displayMax%%", String(App::MAX_DEVICES));
    return page;
  }

  void handleRoot() { App::server.send(200, "text/html", renderPage()); }

  void doFactoryReset() {
    if (LittleFS.exists(ConfigStore::CONF_PATH)) LittleFS.remove(ConfigStore::CONF_PATH);
    WiFiManager wm; wm.resetSettings();

    if (App::cfg.mqttHost.length()) {
      App::mqtt.setServer(App::cfg.mqttHost.c_str(), App::cfg.mqttPort);
      if (App::mqtt.connect("NerdDisplay-ResetFlag")) {
        App::mqtt.publish((App::baseTopic + "has_init").c_str(), "", true);
        App::mqtt.disconnect();
      }
    }
    delay(200);
    ESP.restart();
  }

  void handleFactoryReset() {
    if (App::server.method() != HTTP_POST) { App::server.send(405, "text/plain", "Method Not Allowed"); return; }
    const String apName = "Nerd-Display-" + App::deviceId;
    App::server.send(200, "text/html",
      "<!doctype html><meta charset='utf-8'><meta http-equiv='refresh' content='10'>"
      "<style>body{font-family:sans-serif;text-align:center;margin-top:48px}</style>"
      "<h2>Werksreset wird ausgeführt…</h2>"
      "<p>Neuer AP-Name: <b>" + apName + "</b></p>"
      "<p>Bitte nach ~10 Sekunden erneut verbinden.</p>");
    delay(2000);
    doFactoryReset();
  }

  void handleSave() {
    if (App::server.method() != HTTP_POST) { App::server.send(405, "text/plain", "Method Not Allowed"); return; }

    String oldBase = App::cfg.mqttBase;

    App::cfg.mqttHost = App::server.arg("mqttHost");
    App::cfg.mqttPort = (uint16_t)App::server.arg("mqttPort").toInt();
    App::cfg.mqttUser = App::server.arg("mqttUser");
    App::cfg.mqttPass = App::server.arg("mqttPass");
    App::cfg.mqttBase = ConfigStore::normalizedBase(App::server.arg("mqttBase"));
    App::cfg.mdnsName = App::server.arg("mdnsName"); App::cfg.mdnsName.trim(); App::cfg.mdnsName.toLowerCase();
    App::cfg.displayCount = App::sanitizedDisplayCount((uint8_t)App::server.arg("displayCount").toInt());

    // Retained im alten Topic entfernen, wenn Base geändert wurde
    if (oldBase != App::cfg.mqttBase && App::cfg.mqttHost.length()) {
      String oldBT = ConfigStore::normalizedBase(oldBase) + "/";
      App::mqtt.setServer(App::cfg.mqttHost.c_str(), App::cfg.mqttPort);
      if (App::mqtt.connect("NerdDisplay-TopicChange")) {
        auto zap = [&](const String& t){ App::mqtt.publish(t.c_str(), "", true); };
        zap(oldBT + "has_init");
        zap(oldBT + "state/brightness");
        zap(oldBT + "state/speed");
        zap(oldBT + "state/dwell");
        zap(oldBT + "state/effect");
        zap(oldBT + "state/effect_in");
        zap(oldBT + "state/effect_out");
        zap(oldBT + "state/text");
        App::mqtt.disconnect();
      }
    }

    ConfigStore::save();

    App::server.send(200, "text/html",
      "<!doctype html><meta charset='utf-8'>"
      "<meta http-equiv='refresh' content='6;url=/' />"
      "<style>body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;background:#0b0f19;color:#e5e7eb;text-align:center;margin-top:60px}</style>"
      "<h2>Gespeichert ✓</h2>"
      "<p>Neustart in 1 Sekunde…</p>"
      "<p><small>Du wirst gleich automatisch zur Konfigurationsseite zurückgeleitet.</small></p>"
    );

    delay(1000);
    ESP.restart();
  }
}

namespace WebHandlers {
  void begin() {
    App::server.on("/", ::handleRoot);
    App::server.on("/save", ::handleSave);
    App::server.on("/factory_reset", ::handleFactoryReset);
    App::server.begin();
  }
  void factoryResetNow() { ::doFactoryReset(); }
}
