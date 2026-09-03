# Nerd-Display (ESP8266 + MD\_Parola + MQTT + WebUI)

Zeigt Texte/Werte auf einer MAX7219-Matrix (FC16) mit konfigurierbarer Modulanzahl an.
Konfiguration per WebUI (LittleFS) **und** MQTT.
Unterstützt globale und per-Message Ein-/Ausblendeffekte sowie individuelle Anzeigedauer (dwell) je Nachricht.

## ✨ Features

* MD\_Parola Effekte (global oder pro Nachricht einzeln)
* Individuelle **dwell-Zeiten** pro Nachricht (ms)
* MQTT:

  * `set/*` (Kommandos)
  * `state/*` (retained States)
  * `meta/*` (Bootstrapping-Infos)
* **Home Assistant MQTT Discovery**: Das Display wird automatisch als MQTT-Gerät angelegt
* Bootstrapping: Setzt beim ersten Connect alle Defaultwerte
* WebUI (LittleFS) für Konfiguration
* Konfigurierbare Anzahl verketteter MAX7219-Module (1..32) via WebUI
* Mehrere Nachrichten in einer Schleife rotierend anzeigen

## 🏠 Home Assistant

Wenn Home Assistant mit demselben MQTT-Broker verbunden ist, veröffentlicht das Nerd-Display beim Verbinden automatisch retained MQTT-Discovery-Konfigurationen unter `homeassistant/.../config`.

In Home Assistant werden dabei folgende Entitäten angelegt:

* **Text** (`text`) – angezeigten Text setzen
* **Helligkeit** (`number`) – 0 bis 15
* **Geschwindigkeit** (`number`) – 1 bis 65535
* **Anzeigedauer** (`number`) – 0 bis 600000 ms
* **Einblendeffekt** (`select`)
* **Ausblendeffekt** (`select`)
* Diagnose-Sensoren für **IP-Adresse**, **mDNS** und **Status**

Alle Instanzen erhalten anhand der Geräte-ID eigene `unique_id`-Werte und erscheinen gemeinsam unter einem MQTT-Gerät. Die bestehenden Nerd-Display MQTT-Topics bleiben unverändert; Home Assistant Discovery kommt nur zusätzlich dazu.

## 📂 Ordnerstruktur

```
NerdDisplay.ino
app.h / app.cpp
config_store.h / config_store.cpp
display_service.h / display_service.cpp
mqtt_service.h / mqtt_service.cpp
mqtt_topics.h
home_assistant_discovery.h / home_assistant_discovery.cpp
web_handlers.h / web_handlers.cpp
webui.h
```

## 📦 Abhängige Libraries

* [ESP8266 Core](https://github.com/esp8266/Arduino)
* [MD\_Parola](https://github.com/MajicDesigns/MD_Parola)
* [MD\_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX)
* [PubSubClient](https://github.com/knolleary/pubsubclient)
* [ArduinoJson](https://arduinojson.org/)
* [WiFiManager](https://github.com/tzapu/WiFiManager)
* LittleFS (integriert im ESP8266 Core)

## 📡 MQTT Beispiele

### Einzelner Text (globaler Effekt + dwell)

* **Topic:**
  `<baseTopic>/set/text`
* **Payload:**

  ```json
  "Hallo"
  ```

  oder

  ```json
  {"text":"Hallo"}
  ```

### Mehrere Texte (globaler Effekt + dwell)

```json
["Hallo", "Zebra"]
```

oder

```json
{"messages":["Hallo", "Zebra"]}
```

### Per-Message Effekte und dwell

```json
[
  {"text":"Hallo","in":"SCROLL_LEFT","out":"OPENING","dwell":1000},
  {"text":"Zebra","in":"PRINT","out":"CLOSING","dwell":3000},
  "Eintrag ohne Objekt nutzt globale Defaults"
]
```

* **`in`** = MD\_Parola Effektname für Einblenden (optional, leer = globaler Effekt)
* **`out`** = MD\_Parola Effektname für Ausblenden (optional, leer = globaler Effekt)
* **`dwell`** = Anzeigedauer in Millisekunden (optional, `-1` oder leer = globaler dwell)

### Objektform mit `messages`-Array

```json
{"messages":[
  {"text":"A","dwell":500},
  {"text":"B","in":"SCROLL_RIGHT","dwell":2000}
]}
```

## ⚙ Globale Parameter per MQTT setzen

* `<baseTopic>/set/brightness` → 0..15
* `<baseTopic>/set/speed` → Parola Geschwindigkeit (1..65535)
* `<baseTopic>/set/dwell` → globaler dwell in ms
* `<baseTopic>/set/effect_in` → globaler Einblendeffekt
* `<baseTopic>/set/effect_out` → globaler Ausblendeffekt
* `<baseTopic>/set/effect` → beide Effekte gleichzeitig setzen (Legacy)

## 📜 MD\_Parola Effektliste

Folgende Werte sind für `in` und `out` gültig (Großschreibung beachten):

* PRINT
* SLICE
* WIPE
* WIPE\_CURSOR
* OPENING
* OPENING\_CURSOR
* CLOSING
* CLOSING\_CURSOR
* BLINDS
* DISSOLVE
* SCROLL\_UP
* SCROLL\_DOWN
* SCROLL\_LEFT
* SCROLL\_RIGHT
* SCROLL\_UP\_LEFT
* SCROLL\_UP\_RIGHT
* SCROLL\_DOWN\_LEFT
* SCROLL\_DOWN\_RIGHT
* SCAN\_HORIZ
* SCAN\_VERT
* GROW\_UP
* GROW\_DOWN

*(Die genaue Wirkung dieser Effekte ist in der [MD\_Parola Dokumentation](https://majicdesigns.github.io/MD_Parola/class_m_d___parola.html) beschrieben.)*

## 📜 Lizenz

MIT (siehe LICENSE)
