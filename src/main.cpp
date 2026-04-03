#include <M5Unified.h>

#include "SynthApp.h"

namespace {

SynthApp app;

}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  M5.Display.setFont(&fonts::Font4);

  app.begin();
}

void loop() {
  M5.update();
  app.update();
  delay(5);
}
