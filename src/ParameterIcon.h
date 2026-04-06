#pragma once

#include "Rect.h"

#include <M5Unified.h>

class ParameterIcon {
 public:
  void begin(const Rect& bounds);
  void drawBar(const char* label, float value, bool selected);

  [[nodiscard]] bool contains(int x, int y) const;

 private:
  Rect bounds_{};
  LGFX_Sprite sprite_{&M5.Display};
  bool initialized_ = false;
};
