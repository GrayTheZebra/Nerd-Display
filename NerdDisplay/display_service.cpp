#include "display_service.h"
#include "app.h"

#include <ESP8266WiFi.h>
#include <MD_Parola.h>

// ========================
// Effekt-Helfer
// ========================
struct EffItem { const char* name; textEffect_t eff; };

namespace {
#ifdef MAX_ZONES
  constexpr uint8_t DISPLAY_ZONE_LIMIT = MAX_ZONES;
#else
  constexpr uint8_t DISPLAY_ZONE_LIMIT = 4;
#endif

  // ====== Sonderzeichen (Umlaute + Grad + ß) ======
  constexpr uint8_t CHAR_AE_UPPER = 0x80;
  constexpr uint8_t CHAR_AE_LOWER = 0x81;
  constexpr uint8_t CHAR_OE_UPPER = 0x82;
  constexpr uint8_t CHAR_OE_LOWER = 0x83;
  constexpr uint8_t CHAR_UE_UPPER = 0x84;
  constexpr uint8_t CHAR_UE_LOWER = 0x85;
  constexpr uint8_t CHAR_SHARP_S  = 0x86;
  constexpr uint8_t CHAR_DEGREE   = 0x87;

  // Muss über die gesamte Anzeige-Animation gültig bleiben
  String gMatrixTextBuffer;
  std::vector<String> gZoneTextBuffers;

  // ====== Glyphen ======
  uint8_t fontCharAeUpper[] = { 5, 0x79, 0x14, 0x12, 0x14, 0x79 }; // Ä
  uint8_t fontCharAeLower[] = { 5, 0x20, 0x55, 0x54, 0x79, 0x40 }; // ä
  uint8_t fontCharOeUpper[] = { 5, 0x3D, 0x42, 0x42, 0x42, 0x3D }; // Ö
  uint8_t fontCharOeLower[] = { 5, 0x38, 0x45, 0x44, 0x45, 0x38 }; // ö
  uint8_t fontCharUeUpper[] = { 5, 0x3D, 0x40, 0x40, 0x40, 0x3D }; // Ü
  uint8_t fontCharUeLower[] = { 5, 0x3C, 0x41, 0x40, 0x21, 0x7C }; // ü
  uint8_t fontCharDegree[]  = { 3, 0x06, 0x09, 0x06 };             // °

  void installSpecialChars() {
    App::matrix->addChar(CHAR_AE_UPPER, fontCharAeUpper);
    App::matrix->addChar(CHAR_AE_LOWER, fontCharAeLower);
    App::matrix->addChar(CHAR_OE_UPPER, fontCharOeUpper);
    App::matrix->addChar(CHAR_OE_LOWER, fontCharOeLower);
    App::matrix->addChar(CHAR_UE_UPPER, fontCharUeUpper);
    App::matrix->addChar(CHAR_UE_LOWER, fontCharUeLower);
    App::matrix->addChar(CHAR_DEGREE,   fontCharDegree);
  }

  String matrixTextFromUtf8(const String& in) {
    String out;
    out.reserve(in.length());

    auto appendMapped = [&](uint8_t code) { out += (char)code; };

    for (size_t i = 0; i < in.length(); i++) {
      const uint8_t c = (uint8_t)in[i];

      // UTF-8 C3: ÄÖÜäöüß
      if (c == 0xC3 && (i + 1) < in.length()) {
        const uint8_t n = (uint8_t)in[i + 1];
        if (n == 0x84) { appendMapped(CHAR_AE_UPPER); i++; continue; } // Ä
        if (n == 0x96) { appendMapped(CHAR_OE_UPPER); i++; continue; } // Ö
        if (n == 0x9C) { appendMapped(CHAR_UE_UPPER); i++; continue; } // Ü
        if (n == 0xA4) { appendMapped(CHAR_AE_LOWER); i++; continue; } // ä
        if (n == 0xB6) { appendMapped(CHAR_OE_LOWER); i++; continue; } // ö
        if (n == 0xBC) { appendMapped(CHAR_UE_LOWER); i++; continue; } // ü
      }

      // UTF-8 C2 B0 => °
      if (c == 0xC2 && (i + 1) < in.length() && (uint8_t)in[i + 1] == 0xB0) {
        appendMapped(CHAR_DEGREE);
        i++;
        continue;
      }

      // Default: ASCII durchreichen
      out += (char)c;
    }

    return out;
  }

  const char* renderTextPtr(const String& in) {
    gMatrixTextBuffer = matrixTextFromUtf8(in);
    return gMatrixTextBuffer.c_str();
  }

  const char* renderZoneTextPtr(size_t zoneId, const String& in) {
    if (gZoneTextBuffers.size() <= zoneId) gZoneTextBuffers.resize(zoneId + 1);
    gZoneTextBuffers[zoneId] = matrixTextFromUtf8(in);
    return gZoneTextBuffers[zoneId].c_str();
  }

  bool isZonedMessage(const MessageItem& m) {
    return (m.zone_from > 0 && m.zone_to > 0);
  }

  bool zonesOverlap(const MessageItem& a, const MessageItem& b) {
    if (!isZonedMessage(a) || !isZonedMessage(b)) return true;
    return !(a.zone_to < b.zone_from || b.zone_to < a.zone_from);
  }

  uint16_t dwellFromMessage(const MessageItem& m) {
    uint32_t chosen = (m.dwell_ms >= 0) ? (uint32_t)m.dwell_ms : App::params.dwell;
    return (chosen > 65535U) ? 65535U : (uint16_t)chosen;
  }
}

// ========================
// Effekte
// ========================
template<typename T, size_t N>
static textEffect_t effFromName(const T (&arr)[N], const String& n, textEffect_t fallback) {
  String u = n; u.trim(); u.toUpperCase();
  for (size_t i = 0; i < N; i++) if (u == arr[i].name) return arr[i].eff;
  return fallback;
}

static const EffItem EFFECTS_IN[] PROGMEM = {
  {"PRINT",          PA_PRINT},
  {"SCROLL_DOWN",    PA_SCROLL_DOWN},
  {"SCROLL_UP",      PA_SCROLL_UP},
  {"SCROLL_LEFT",    PA_SCROLL_LEFT},
  {"SCROLL_RIGHT",   PA_SCROLL_RIGHT},
  {"OPENING",        PA_OPENING},
  {"OPENING_CURSOR", PA_OPENING_CURSOR},
};

static const EffItem EFFECTS_OUT[] PROGMEM = {
  {"NO_EFFECT",       PA_NO_EFFECT},
  {"SCROLL_DOWN",     PA_SCROLL_DOWN},
  {"SCROLL_UP",       PA_SCROLL_UP},
  {"SCROLL_LEFT",     PA_SCROLL_LEFT},
  {"SCROLL_RIGHT",    PA_SCROLL_RIGHT},
  {"CLOSING",         PA_CLOSING},
  {"CLOSING_CURSOR",  PA_CLOSING_CURSOR},
};

static textEffect_t effectFromNameIn (const String& n) { return effFromName(EFFECTS_IN,  n, PA_SCROLL_LEFT); }
static textEffect_t effectFromNameOut(const String& n) { return effFromName(EFFECTS_OUT, n, PA_SCROLL_LEFT); }

// ========================
// Display-API
// ========================
namespace Display {

  void begin() {
    if (App::matrix == nullptr) return;
    App::matrix->begin();
    installSpecialChars();
    applyParams();
    App::matrix->displayClear();
  }

  void applyParams() {
    if (App::matrix == nullptr) return;
    App::matrix->setIntensity(App::params.brightness);
  }

  void showImmediate(const String& s, uint32_t dwellMs) {
    if (App::matrix == nullptr) return;
    uint16_t pause = (dwellMs > 65535U) ? 65535U : (uint16_t)dwellMs;
    App::matrix->displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause, PA_PRINT, PA_NO_EFFECT);
    App::matrix->displayReset();
    App::matrix->displayAnimate();
  }

  void startWith(const String& s) {
    if (App::matrix == nullptr) return;
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix->displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(App::params.effect_in),
                            effectFromNameOut(App::params.effect_out));
  }

  void startWith(const String& s, const String& effInName, const String& effOutName) {
    if (App::matrix == nullptr) return;
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix->displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  void startWith(const String& s, const String& effInName, const String& effOutName, int32_t dwellOverrideMs) {
    if (App::matrix == nullptr) return;
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;

    uint32_t chosen = (dwellOverrideMs >= 0) ? (uint32_t)dwellOverrideMs : App::params.dwell;
    uint16_t pause  = (chosen > 65535U) ? 65535U : (uint16_t)chosen;

    App::matrix->displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  void nextMessage() {
    if (App::params.messages.empty()) return;

    const size_t total = App::params.messages.size();
    const size_t startIdx = App::msgIndex;
    const MessageItem& first = App::params.messages[startIdx];

    // Standardfall: kein Bereich angegeben -> komplette Anzeige wie bisher
    if (!isZonedMessage(first)) {
      startWith(first.text, first.eff_in, first.eff_out, first.dwell_ms);
      App::msgIndex = (App::msgIndex + 1) % total;
      return;
    }

    // Bereichsmodus:
    // - aufeinanderfolgende nicht-überlappende Bereiche gleichzeitig anzeigen
    // - sobald eine Überlappung auftritt, im nächsten Zyklus fortsetzen (nacheinander)
    std::vector<size_t> group;
    group.reserve(total);
    group.push_back(startIdx);

    size_t idx = (startIdx + 1) % total;
    while (idx != startIdx) {
      const MessageItem& candidate = App::params.messages[idx];
      if (!isZonedMessage(candidate)) break;
      if (group.size() >= DISPLAY_ZONE_LIMIT) break;

      bool hasOverlap = false;
      for (size_t gidx : group) {
        if (zonesOverlap(App::params.messages[gidx], candidate)) {
          hasOverlap = true;
          break;
        }
      }
      if (hasOverlap) break;

      group.push_back(idx);
      idx = (idx + 1) % total;
    }

    if (App::matrix == nullptr) return;

    for (size_t zoneId = 0; zoneId < group.size(); ++zoneId) {
      const MessageItem& m = App::params.messages[group[zoneId]];
      const uint8_t firstModule = (uint8_t)constrain(m.zone_from - 1, 0, (int)App::cfg.displayCount - 1);
      const uint8_t lastModule  = (uint8_t)constrain(m.zone_to   - 1, 0, (int)App::cfg.displayCount - 1);
      const String inName  = m.eff_in.length()  ? m.eff_in  : App::params.effect_in;
      const String outName = m.eff_out.length() ? m.eff_out : App::params.effect_out;

      App::matrix->setZone((uint8_t)zoneId, firstModule, lastModule);
      App::matrix->displayZoneText((uint8_t)zoneId, renderZoneTextPtr(zoneId, m.text), PA_CENTER, App::params.speed,
                                   dwellFromMessage(m), effectFromNameIn(inName), effectFromNameOut(outName));
    }
    App::matrix->displayReset();

    App::msgIndex = (App::msgIndex + group.size()) % total;
  }

  void startInfoScroll() {
    if (App::matrix == nullptr) return;
    const String combined = WiFi.localIP().toString() + ", " + App::mdnsHost + ".local, ";
    static char infoText[128];
    combined.toCharArray(infoText, sizeof(infoText));
    App::matrix->displayText(infoText, PA_CENTER, 40, 0, PA_SCROLL_LEFT, PA_NO_EFFECT);
    App::matrix->displayReset();
  }

  bool animateOnce() {
    if (App::matrix == nullptr) return false;
    return App::matrix->displayAnimate();
  }
}
