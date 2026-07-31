#pragma once

#include <cstdint>

// Polls the Tab5 IO expander (PI4IOE5V6408 @0x43) for headphone insertion
// (HP_DET) and gates the speaker amplifier (SPK_EN) so that inserting
// headphones mutes the built-in speaker at any time.
class HeadphoneDetector {
 public:
  void poll(std::uint32_t now_ms);
  bool headphonePresent() const { return headphone_present_; }

 private:
  bool headphone_present_ = false;
  bool state_known_ = false;
  std::uint32_t last_poll_ms_ = 0;
};
