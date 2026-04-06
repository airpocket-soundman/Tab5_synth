#include "AudioSourceType.h"

const char* audioSourceLabel(AudioSourceType source_type) {
  switch (source_type) {
    case AudioSourceType::Oscillator:
      return "OSC";
    case AudioSourceType::OnboardMic:
      return "MIC";
    case AudioSourceType::ExternalI2S:
      return "I2S";
    default:
      return "OSC";
  }
}
