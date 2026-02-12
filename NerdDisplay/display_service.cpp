#include "display_service.h"
#include "app.h"

#include <ESP8266WiFi.h>
#include <MD_Parola.h>

// ========================
// Effekt-Helfer
// ========================
struct EffItem { const char* name; textEffect_t eff; };

namespace {
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

  // ====== Glyphen ======
  uint8_t fontCharAeUpper[] = { 5, 0x79, 0x14, 0x12, 0x14, 0x79 }; // Ä
  uint8_t fontCharAeLower[] = { 5, 0x20, 0x55, 0x54, 0x79, 0x40 }; // ä
  uint8_t fontCharOeUpper[] = { 5, 0x3D, 0x42, 0x42, 0x42, 0x3D }; // Ö
  uint8_t fontCharOeLower[] = { 5, 0x38, 0x45, 0x44, 0x45, 0x38 }; // ö
  uint8_t fontCharUeUpper[] = { 5, 0x3D, 0x40, 0x40, 0x40, 0x3D }; // Ü
  uint8_t fontCharUeLower[] = { 5, 0x3C, 0x41, 0x40, 0x21, 0x7C }; // ü
  uint8_t fontCharDegree[]  = { 3, 0x06, 0x09, 0x06 };             // °

  void installSpecialChars() {
    App::matrix.addChar(CHAR_AE_UPPER, fontCharAeUpper);
    App::matrix.addChar(CHAR_AE_LOWER, fontCharAeLower);
    App::matrix.addChar(CHAR_OE_UPPER, fontCharOeUpper);
    App::matrix.addChar(CHAR_OE_LOWER, fontCharOeLower);
    App::matrix.addChar(CHAR_UE_UPPER, fontCharUeUpper);
    App::matrix.addChar(CHAR_UE_LOWER, fontCharUeLower);
    App::matrix.addChar(CHAR_DEGREE,   fontCharDegree);
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
    App::matrix.begin();
    installSpecialChars();
    applyParams();
    App::matrix.displayClear();
  }

  void applyParams() {
    App::matrix.setIntensity(App::params.brightness);
  }

  void showImmediate(const String& s, uint32_t dwellMs) {
    uint16_t pause = (dwellMs > 65535U) ? 65535U : (uint16_t)dwellMs;
    App::matrix.displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause, PA_PRINT, PA_NO_EFFECT);
    App::matrix.displayReset();
    App::matrix.displayAnimate();
  }

  void startWith(const String& s) {
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix.displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(App::params.effect_in),
                            effectFromNameOut(App::params.effect_out));
  }

  void startWith(const String& s, const String& effInName, const String& effOutName) {
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix.displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  void startWith(const String& s, const String& effInName, const String& effOutName, int32_t dwellOverrideMs) {
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;

    uint32_t chosen = (dwellOverrideMs >= 0) ? (uint32_t)dwellOverrideMs : App::params.dwell;
    uint16_t pause  = (chosen > 65535U) ? 65535U : (uint16_t)chosen;

    App::matrix.displayText(renderTextPtr(s), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  void nextMessage() {
    if (App::params.messages.empty()) return;
    const MessageItem& m = App::params.messages[App::msgIndex];
    startWith(m.text, m.eff_in, m.eff_out, m.dwell_ms);
    App::msgIndex = (App::msgIndex + 1) % App::params.messages.size();
  }

  void startInfoScroll() {
    const String combined = WiFi.localIP().toString() + ", " + App::mdnsHost + ".local, ";
    static char infoText[128];
    combined.toCharArray(infoText, sizeof(infoText));
    App::matrix.displayText(infoText, PA_CENTER, 40, 0, PA_SCROLL_LEFT, PA_NO_EFFECT);
    App::matrix.displayReset();
  }

  bool animateOnce() {
    return App::matrix.displayAnimate();
  }
}
