#pragma once

#include "AudioSourceType.h"
#include "Waveform.h"

#include <cstddef>

class AudioSource {
 public:
  virtual ~AudioSource() = default;

  virtual AudioSourceType type() const = 0;
  virtual bool begin() = 0;
  virtual bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) = 0;
  virtual void noteOff(std::size_t voice_index) = 0;
  virtual void noteOffAll() = 0;
  virtual void setVolume(float volume) = 0;
  virtual void setVoiceLevel(std::size_t voice_index, float level, Waveform waveform) = 0;
  virtual bool isAvailable() const = 0;
};
