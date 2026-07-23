#pragma once

#include <cstdint>

enum class InstrumentTimbre : std::uint8_t {
  Basic = 0,
  Guitar,
  Piano,
  Organ,
  Recorder,
  Pad,
  Pluck,
  Bell,
  Brass,
  Bass,
  Synth,
};
