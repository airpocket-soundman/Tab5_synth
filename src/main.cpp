#include <M5Unified.h>

#include "RetroSynthApp.h"

namespace {

RetroSynthApp app;

}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Display.setRotation(3);
  app.begin();
}

void loop() {
  M5.update();
  app.update();
  delay(1);
}
