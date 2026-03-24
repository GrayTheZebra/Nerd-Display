#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <vector>

struct PersistentConfig {
  String   mqttHost;
  uint16_t mqttPort = 1883;
  String   mqttUser;
  String   mqttPass;
  String   mqttBase;   // ohne trailing Slash
  String   mdnsName;   // kurzer Suffix
  uint8_t  displayCount = 4; // Anzahl MAX7219-Module in Reihe
};

struct MessageItem {
  String text;
  String eff_in;   // optional, leer => globale Defaults
  String eff_out;  // optional, leer => globale Defaults
  int32_t dwell_ms = -1;  // optional; -1 => App::params.dwell verwenden
};

struct RuntimeParams {
  uint8_t  brightness;   // 0..15
  uint16_t speed;        // Parola speed
  uint32_t dwell;        // ms
  String   effect_in;    // globale Defaults
  String   effect_out;   // globale Defaults
  std::vector<MessageItem> messages;   // <-- statt vector<String>
};

namespace App {
  // Hardware-Konstante/n
  static const MD_MAX72XX::moduleType_t HARDWARE_TYPE = MD_MAX72XX::FC16_HW;
  static const uint8_t MIN_DEVICES = 1;
  static const uint8_t MAX_DEVICES = 32;
  uint8_t sanitizedDisplayCount(uint8_t requested);
  static const uint8_t PIN_CS = D2;

  // Globale Instanzen
  extern MD_Parola* matrix;
  extern ESP8266WebServer server;
  extern WiFiClient net;
  extern PubSubClient mqtt;

  extern PersistentConfig cfg;
  extern RuntimeParams params;

  extern String deviceId;      // "4FA2CB"
  extern String baseTopic;     // "zebra/nerd-display/<MAC>/"
  extern String mdnsHost;      // "nerd-display-<mdns>"

  extern volatile bool setBootstrapped;
  extern volatile bool hasInitFlag;

  extern bool ipScrollMode;
  extern uint8_t msgIndex;
  extern unsigned long lastMqttReconnect;

  // Defaults für Runtime
  void setRuntimeDefaults();
}
