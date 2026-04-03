#pragma once

struct Rect {
  int x;
  int y;
  int w;
  int h;

  [[nodiscard]] bool contains(int px, int py) const {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }
};
