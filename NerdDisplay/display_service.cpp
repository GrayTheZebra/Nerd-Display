#include "display_service.h"
#include "app.h"

#include <ESP8266WiFi.h>
#include <MD_Parola.h>

// ========================
// Effekt-Helfer
// ========================
struct EffItem { const char* name; textEffect_t eff; };

namespace {
  constexpr char CHAR_AE_UPPER    = 1;
  constexpr char CHAR_AE_LOWER    = 2;
  constexpr char CHAR_OE_UPPER    = 3;
  constexpr char CHAR_OE_LOWER    = 4;
  constexpr char CHAR_UE_UPPER    = 5;
  constexpr char CHAR_UE_LOWER    = 6;
  constexpr char CHAR_SHARP_S     = 7;
  constexpr char CHAR_DEGREE      = 8;

  // 5x7 Fontdaten (Breite + Spaltenbytes)
  uint8_t fontCharAeUpper[] = { 5, 0x7d, 0x12, 0x11, 0x12, 0x7d };
  uint8_t fontCharAeLower[] = { 5, 0x24, 0x54, 0x54, 0x54, 0x7c };
  uint8_t fontCharOeUpper[] = { 5, 0x3e, 0x41, 0x41, 0x41, 0x3e };
  uint8_t fontCharOeLower[] = { 5, 0x38, 0x44, 0x44, 0x44, 0x38 };
  uint8_t fontCharUeUpper[] = { 5, 0x3f, 0x40, 0x40, 0x40, 0x3f };
  uint8_t fontCharUeLower[] = { 5, 0x3c, 0x40, 0x40, 0x20, 0x7c };
  uint8_t fontCharSharpS[]  = { 5, 0x24, 0x56, 0x55, 0x54, 0x48 };
  uint8_t fontCharDegree[]  = { 3, 0x06, 0x09, 0x06 };

  void installSpecialChars() {
    App::matrix.addChar((uint8_t)CHAR_AE_UPPER, fontCharAeUpper);
    App::matrix.addChar((uint8_t)CHAR_AE_LOWER, fontCharAeLower);
    App::matrix.addChar((uint8_t)CHAR_OE_UPPER, fontCharOeUpper);
    App::matrix.addChar((uint8_t)CHAR_OE_LOWER, fontCharOeLower);
    App::matrix.addChar((uint8_t)CHAR_UE_UPPER, fontCharUeUpper);
    App::matrix.addChar((uint8_t)CHAR_UE_LOWER, fontCharUeLower);
    App::matrix.addChar((uint8_t)CHAR_SHARP_S,  fontCharSharpS);
    App::matrix.addChar((uint8_t)CHAR_DEGREE,   fontCharDegree);
  }

  String matrixTextFromUtf8(const String& in) {
    String out;
    out.reserve(in.length());

    for (size_t i = 0; i < in.length(); i++) {
      const uint8_t c = (uint8_t)in[i];

      // UTF-8 Leadbyte C3: ÄÖÜäöüßẞ
      if (c == 0xC3 && (i + 1) < in.length()) {
        const uint8_t n = (uint8_t)in[i + 1];
        if (n == 0x84) { out += CHAR_AE_UPPER; i++; continue; } // Ä
        if (n == 0x96) { out += CHAR_OE_UPPER; i++; continue; } // Ö
        if (n == 0x9C) { out += CHAR_UE_UPPER; i++; continue; } // Ü
        if (n == 0xA4) { out += CHAR_AE_LOWER; i++; continue; } // ä
        if (n == 0xB6) { out += CHAR_OE_LOWER; i++; continue; } // ö
        if (n == 0xBC) { out += CHAR_UE_LOWER; i++; continue; } // ü
        if (n == 0x9F || n == 0x9E) { out += CHAR_SHARP_S; i++; continue; } // ß/ẞ
      }

      // UTF-8 C2 B0 => °
      if (c == 0xC2 && (i + 1) < in.length() && (uint8_t)in[i + 1] == 0xB0) {
        out += CHAR_DEGREE;
        i++;
        continue;
      }

      out += (char)c;
    }

    return out;
  }
}

template<typename T, size_t N>
static textEffect_t effFromName(const T (&arr)[N], const String& n, textEffect_t fallback) {
  String u = n; u.trim(); u.toUpperCase();
  for (size_t i = 0; i < N; i++) if (u == arr[i].name) return arr[i].eff;
  return fallback;
}

// Effekt-Tabellen
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

static textEffect_t effectFromNameIn (const String& n)  { return effFromName(EFFECTS_IN,  n, PA_SCROLL_LEFT); }
static textEffect_t effectFromNameOut(const String& n)  { return effFromName(EFFECTS_OUT, n, PA_SCROLL_LEFT); }

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
    const String displayText = matrixTextFromUtf8(s);
    // MD_Parola erwartet pause als uint16_t; clampen
    uint16_t pause = (dwellMs > 65535U) ? 65535U : (uint16_t)dwellMs;
    App::matrix.displayText(displayText.c_str(), PA_CENTER, App::params.speed, pause, PA_PRINT, PA_NO_EFFECT);
    App::matrix.displayReset();
    App::matrix.displayAnimate();
  }

  // Variante ohne Effekte: globale Defaults + globaler dwell
  void startWith(const String& s) {
    const String displayText = matrixTextFromUtf8(s);
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix.displayText(displayText.c_str(), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(App::params.effect_in),
                            effectFromNameOut(App::params.effect_out));
  }

  // Variante mit per-Message Effekten, aber globalem dwell
  void startWith(const String& s, const String& effInName, const String& effOutName) {
    const String displayText = matrixTextFromUtf8(s);
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;
    uint16_t pause = (App::params.dwell > 65535U) ? 65535U : (uint16_t)App::params.dwell;
    App::matrix.displayText(displayText.c_str(), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  // NEU: per-Message Effekte + per-Message dwell (Override)
  void startWith(const String& s, const String& effInName, const String& effOutName, int32_t dwellOverrideMs) {
    const String displayText = matrixTextFromUtf8(s);
    const String inName  = effInName.length()  ? effInName  : App::params.effect_in;
    const String outName = effOutName.length() ? effOutName : App::params.effect_out;

    uint32_t base = App::params.dwell;
    uint32_t chosen = (dwellOverrideMs >= 0) ? (uint32_t)dwellOverrideMs : base;
    uint16_t pause = (chosen > 65535U) ? 65535U : (uint16_t)chosen;

    App::matrix.displayText(displayText.c_str(), PA_CENTER, App::params.speed, pause,
                            effectFromNameIn(inName),
                            effectFromNameOut(outName));
  }

  void nextMessage() {
    if (App::params.messages.empty()) return;
    const MessageItem& m = App::params.messages[App::msgIndex];
    startWith(m.text, m.eff_in, m.eff_out, m.dwell_ms);  // nutzt Override falls gesetzt
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
