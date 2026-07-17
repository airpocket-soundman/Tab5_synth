#pragma once

#include <M5GFX.h>

using LGFX_Sprite = lgfx::LGFX_Sprite;

struct M5UnifiedShim {
  M5GFX Display;
};

extern M5UnifiedShim M5;
