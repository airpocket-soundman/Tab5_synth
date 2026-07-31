#include "HeadphoneDetector.h"

#include "SynthConfig.h"

#if defined(ESP_PLATFORM)
#include <M5Unified.h>
#endif

namespace {

// PI4IOE5V6408 (I2C 0x43) pin assignment on Tab5. See M5Unified
// Power_Class.cpp: bit7 = HP_DET (input), bit1 = SPK_EN (output).
constexpr std::uint8_t kHpDetPin = 7;
constexpr std::uint8_t kSpkEnPin = 1;

}  // namespace

void HeadphoneDetector::poll(std::uint32_t now_ms) {
#if defined(ESP_PLATFORM)
  if (M5.getBoard() != m5::board_t::board_M5Tab5) {
    return;
  }
  if (state_known_ && (now_ms - last_poll_ms_) < SynthConfig::audio.hp_detect_poll_ms) {
    return;
  }
  last_poll_ms_ = now_ms;

  auto& ioexp = M5.getIOExpander(0);
  const bool level = ioexp.digitalRead(kHpDetPin);
  const bool present = SynthConfig::audio.hp_detect_active_high ? level : !level;

  if (!state_known_ || present != headphone_present_) {
    state_known_ = true;
    headphone_present_ = present;
    ioexp.digitalWrite(kSpkEnPin, !present);
    Serial.printf("[HP] det_level=%d headphone=%s speaker=%s\n", level ? 1 : 0,
                  present ? "in" : "out", present ? "off" : "on");
  } else if (present && ioexp.getWriteValue(kSpkEnPin)) {
    // M5Unified re-asserts SPK_EN whenever the speaker restarts; keep it
    // gated while headphones remain inserted.
    ioexp.digitalWrite(kSpkEnPin, false);
  }
#else
  (void)now_ms;
#endif
}
