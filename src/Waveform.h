#pragma once

#include <M5Unified.h>

#include <cstdint>

enum class Waveform : std::uint8_t {
  Sine = 0,
  Saw,
  Square,
  Triangle,
  Count,
};

const char* waveformLabel(Waveform waveform);
std::uint32_t waveformColor(Waveform waveform);
