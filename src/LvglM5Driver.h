#pragma once

class LvglM5Driver {
 public:
  bool begin();
  void update();
  void renderNow();
  void setRenderingEnabled(bool enabled);
};
