/*
  Nerd-Display (ESP8266 + MD_Parola + MQTT + WebUI)
  Modularisierte Hauptdatei (Orchestrierung)
*/

#include <Arduino.h>
#include <SPI.h>
#include <MD_Parola.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <WiFiManager.h>

#include "app.h"
#include "config_store.h"
#include "display_service.h"
#include "mqtt_service.h"
#include "web_handlers.h"

void setup() {
  Serial.begin(115200);
  delay(50);

  // LittleFS mounten und Konfig laden
  if (!LittleFS.begin()) {
    Serial.println(F("[LittleFS] Mount fehlgeschlagen."));
  } else {
    ConfigStore::load();
  }

  // Eindeutige ID aus MAC
  App::deviceId = ConfigStore::idFromMAC();

  // Display initialisieren + Runtime-Defaults
  Display::begin();
  App::setRuntimeDefaults();
  Display::applyParams();

  // Persistente Defaults (mdnsName, mqttBase) sicherstellen
  ConfigStore::ensurePersistentDefaults();

  // Kurzer AP-Hinweis am Display
  Display::showImmediate("AP", 0);

  // WiFiManager: AP + AutoConnect
  WiFiManager wm;
  const String apName = "Nerd-Display-" + App::deviceId;
  wm.setHostname(apName);
  wm.setDebugOutput(false);
  if (!wm.autoConnect(apName.c_str())) {
    // Fallback ins Portal (blockierend, bis gespeichert/verbunden)
    wm.startConfigPortal(apName.c_str());
  }

  // mDNS starten
  App::mdnsHost = "nerd-display-" + App::cfg.mdnsName;
  if (MDNS.begin(App::mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println(F("[mDNS] Start fehlgeschlagen."));
  }

  // MQTT optional: wenn kein Host gesetzt, Info-Scroll anzeigen
  App::ipScrollMode = (App::cfg.mqttHost.length() == 0);
  if (App::ipScrollMode) {
    Display::startInfoScroll();
  }

  // Basis-Topic bilden
  App::baseTopic = ConfigStore::makeBaseTopic();

  // Webserver-Routen registrieren
  WebHandlers::begin();

  // MQTT vorbereiten (Callback, Buffer)
  Mqtt::begin();

  // Falls MQTT konfiguriert: verbinden und erste Nachricht anzeigen
  if (!App::ipScrollMode) {
    Mqtt::reconnect();
    Display::nextMessage();
  }
}

void loop() {
  // Modus ohne MQTT: nur Info-Scroll + Webserver bedienen
  if (App::ipScrollMode) {
    if (Display::animateOnce()) App::matrix.displayReset();
    App::server.handleClient();
    return;
  }

  // Normale Anzeige: bei Ende der Animation zur nächsten Message
  if (Display::animateOnce()) Display::nextMessage();

  // Webserver bedienen
  App::server.handleClient();

  // MQTT-Verbindung pflegen/reconnecten
  if (!App::mqtt.connected()) {
    const unsigned long now = millis();
    if (now - App::lastMqttReconnect > 3000) {
      App::lastMqttReconnect = now;
      Mqtt::reconnect();
    }
  } else {
    Mqtt::loop();
  }
}
