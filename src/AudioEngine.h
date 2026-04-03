#pragma once

#include "Waveform.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstddef>
#include <cstdint>

class AudioEngine {
 public:
  void begin();
  void noteOn(int midi_note, Waveform waveform);
  void noteOff();

  [[nodiscard]] bool isNotePlaying() const;
  [[nodiscard]] int activeMidiNote() const;
  [[nodiscard]] float activeFrequency() const;
  [[nodiscard]] Waveform activeWaveform() const;

 private:
  struct RenderState {
    bool gate = false;
    float frequency = 0.0f;
    Waveform waveform = Waveform::Sine;
  };

  static void audioTaskEntry(void* arg);
  void audioTaskLoop();
  void fillBuffer(std::int16_t* buffer, std::size_t sample_count);
  static float midiToFrequency(int midi_note);
  static float waveformSample(Waveform waveform, float phase);

  TaskHandle_t task_handle_ = nullptr;
  portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
  RenderState render_state_{};
  bool note_playing_ = false;
  int active_midi_note_ = -1;
  float active_frequency_ = 0.0f;
  Waveform active_waveform_ = Waveform::Sine;
  float phase_ = 0.0f;
  float level_ = 0.0f;
};
