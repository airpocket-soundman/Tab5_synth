#include <emscripten.h>

#include <M5Unified.h>
#include <lgfx/v1/platforms/sdl/Panel_sdl.hpp>

#include "PerformanceUi.h"

namespace {

constexpr int kDisplayWidth = 1280;
constexpr int kDisplayHeight = 720;

lgfx::Panel_sdl panel;
PerformanceUi performance_ui;

void runFrame() {
  if (lgfx::Panel_sdl::loop() != 0) {
    emscripten_cancel_main_loop();
    lgfx::Panel_sdl::close();
  }
}

}  // namespace

M5UnifiedShim M5;

int main() {
  if (lgfx::Panel_sdl::setup() != 0) {
    return 1;
  }

  auto config = panel.config();
  config.memory_width = kDisplayWidth;
  config.memory_height = kDisplayHeight;
  config.panel_width = kDisplayWidth;
  config.panel_height = kDisplayHeight;
  panel.config(config);
  panel.setWindowTitle("Tab5 Synth M5GFX Preview");

  if (!M5.Display.init(&panel)) {
    lgfx::Panel_sdl::close();
    return 1;
  }

  UiState state{};
  state.oscillator_available = true;
  state.onboard_mic_available = true;
  state.external_i2s_available = true;

  performance_ui.begin();
  performance_ui.drawInitial(state);

  emscripten_set_main_loop(runFrame, 0, true);
  return 0;
}
