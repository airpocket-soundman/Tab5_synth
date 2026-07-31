#include <M5Unified.h>

#include <esp_system.h>

#include "RetroSynthApp.h"

namespace {

RetroSynthApp app;

}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  const esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.printf("[BOOT] reset_reason=%d (%s)\n", static_cast<int>(reset_reason),
                reset_reason == ESP_RST_POWERON     ? "poweron"
                : reset_reason == ESP_RST_SW        ? "software"
                : reset_reason == ESP_RST_PANIC     ? "panic"
                : reset_reason == ESP_RST_INT_WDT   ? "int_wdt"
                : reset_reason == ESP_RST_TASK_WDT  ? "task_wdt"
                : reset_reason == ESP_RST_WDT       ? "other_wdt"
                : reset_reason == ESP_RST_BROWNOUT  ? "brownout"
                : reset_reason == ESP_RST_DEEPSLEEP ? "deepsleep"
                                                    : "other");

  M5.Display.setRotation(3);
  app.begin();
}

void loop() {
  M5.update();
  app.update();
  delay(1);
}
