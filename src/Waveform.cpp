#include "Waveform.h"

const char* waveformLabel(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return "SINE";
    case Waveform::Saw:
      return "SAW";
    case Waveform::Square:
      return "SQUARE";
    case Waveform::Triangle:
      return "TRIANGLE";
    default:
      return "";
  }
}

std::uint32_t waveformColor(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return 0x2EC4B6;
    case Waveform::Saw:
      return 0xFF9F1C;
    case Waveform::Square:
      return 0xE71D36;
    case Waveform::Triangle:
      return 0x5C7CFA;
    default:
      return 0x666666;
  }
}
