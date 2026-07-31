#pragma once

#include "AudioEngine.h"
#include "HeadphoneDetector.h"
#include "LvglM5Driver.h"
#include "lvgl_synth_ui.h"

#include <array>
#include <cstdint>

class RetroSynthApp {
 public:
  void begin();
  void update();

 private:
  static void stateChanged(void* user_data);
  static void noteChanged(float midi_note, bool pressed, void* user_data);
  static void xyNoteChanged(float midi_note, bool pressed, void* user_data);
  static void micRecordingChanged(bool recording, void* user_data);

  void readUiState();
  void applyAudioState(bool include_lfo);
  void processSerialCommands();
  void execSerialCommand(const char* command);
  void updatePressedNotes();
  bool hasActiveLfo() const;
  float modulatedValue(int target, std::uint32_t now_ms) const;
  Waveform selectedWaveform() const;

  LvglM5Driver lvgl_{};
  AudioEngine audio_engine_{};
  HeadphoneDetector headphone_detector_{};
  lvgl_synth_state_t ui_state_{};
  bool mic_recording_ = false;
  bool note_pressed_ = false;
  bool rendering_suspended_ = false;
  bool performance_input_dirty_ = false;
  float active_note_ = 60.0f;
  bool xy_note_active_ = false;
  float xy_note_ = 60.0f;
  std::array<bool, 128> pressed_notes_{};
  std::uint32_t render_resume_ms_ = 0;
  std::uint32_t mic_started_ms_ = 0;
  std::uint32_t last_lfo_update_ms_ = 0;
  char serial_command_[32] = {};
  std::size_t serial_command_len_ = 0;
};
