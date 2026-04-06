#pragma once

#include <cstdint>

enum class AudioSourceType : std::uint8_t {
  Oscillator = 0,
  OnboardMic,
  ExternalI2S,
};

const char* audioSourceLabel(AudioSourceType source_type);
