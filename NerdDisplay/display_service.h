#pragma once
#include <Arduino.h>

namespace Display {
  void begin();                // App::matrix.begin + Grundsetup
  void applyParams();          // Helligkeit etc.
  void showImmediate(const String& s, uint32_t dwellMs = 0);

  // Startvarianten:
  void startWith(const String& s);                                                    // globale Effekte + globaler dwell
  void startWith(const String& s, const String& effInName, const String& effOutName); // per-Message Effekte, globaler dwell
  void startWith(const String& s, const String& effInName, const String& effOutName, int32_t dwellOverrideMs); // per-Message Effekte + per-Message dwell

  void clear();                 // Animation stoppen und Matrix sofort leeren
  void nextMessage();           // nimmt MessageItem & nutzt ggf. dwell-Override
  void startInfoScroll();       // IP + mDNS scrollen
  bool animateOnce();           // true, wenn eine Animation „fertig“ ist
}
