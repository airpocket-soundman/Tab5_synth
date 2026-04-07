#pragma once

#include "Waveform.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace SynthConfig {

struct AudioConfig {
  int speaker_volume = 180;
  int audio_channel = 0;
  std::size_t polyphony_voices = 8;
  float default_volume = 0.45f;
  float center_sample = 128.0f;
  std::size_t wavetable_size = 256;
  float sine_peak = 100.0f;
  float saw_peak = 46.0f;
  float square_peak = 34.0f;
  float triangle_peak = 127.0f;
  float sine_trim = 0.90f;
  float saw_trim = 0.88f;
  float square_trim = 0.82f;
  float triangle_trim = 0.94f;
  std::uint32_t mic_sample_rate = 24000;
  std::uint32_t mic_sample_max_ms = 5000;
  std::uint32_t mic_sample_commit_min_ms = 1000;
  std::int16_t mic_trim_threshold = 1200;
  std::size_t mic_record_block_samples = 256;
  int mic_sample_root_note = 60;
  std::uint32_t external_i2s_sample_rate = 16000;
  std::size_t external_i2s_buffer_frames = 256;
  std::size_t external_i2s_buffer_count = 3;
  float amp_attack_default_ms = 8.0f;
  float amp_decay_default_ms = 180.0f;
  float amp_sustain_default = 0.72f;
  float amp_release_default_ms = 480.0f;
  float amp_attack_max_ms = 3000.0f;
  float amp_decay_max_ms = 3000.0f;
  float amp_release_max_ms = 4000.0f;
};

struct UiConfig {
  std::size_t max_touch_points = 5;
  int selection_button_columns = 6;
  int selection_button_height = 92;
  int selection_button_gap = 10;
  int selection_button_text_size = 1;
  int wave_button_padding = 20;
  int wave_icon_thickness = 3;
  int parameter_button_height = 92;
  int parameter_button_gap = 10;
  int volume_area_height = 74;
  int slider_track_height = 18;
  int slider_inset = 18;
  int volume_value_gap = 12;
  int volume_value_width = 48;
  float performance_area_width_ratio = 0.5f;
  float performance_square_scale = 0.66f;
  int performance_square_top_margin = 20;
  int pitch_mode_gap = 16;
  int pitch_mode_height = 64;
  int pitch_mode_button_gap = 12;
  int keyboard_top_gap = 20;
  int keyboard_side_margin = 20;
  int keyboard_bottom_margin = 20;
  int keyboard_min_height = 60;
  // 鍵盤エリアに割り当てる高さ比率。1.0で利用可能高さすべて、0.5で半分。
  float keyboard_height_scale = 0.5f;
  float keyboard_black_height_ratio = 0.6f;
  float keyboard_black_width_ratio = 0.62f;
  int keyboard_root_note = 48;
  int keyboard_octaves = 2;
  int center_guide_segment = 8;
  int center_guide_gap = 6;
  int min_midi_note = 48;
  int max_midi_note = 96;
  int round_radius = 16;
  int marker_radius = 18;
  std::uint32_t background_color = 0x05070C;
  std::uint32_t panel_color = 0x0B1020;
  std::uint32_t panel_border_color = 0x394867;
  std::uint32_t muted_button_color = 0x1B1B1B;
  std::uint32_t muted_border_color = 0x555555;
  std::uint32_t disabled_button_color = 0x121212;
  std::uint32_t disabled_text_color = 0x707070;
  std::uint32_t muted_text_color = 0xEAEAEA;
  std::uint32_t selected_border_color = 0xF4F1DE;
  std::uint32_t selected_text_color = 0x081018;
  std::uint32_t slider_fill_color = 0x4CC9F0;
  std::uint32_t slider_track_color = 0x162033;
  std::uint32_t keyboard_white_color = 0xF1F2F4;
  std::uint32_t keyboard_black_color = 0x0E1118;
  std::uint32_t keyboard_line_color = 0x2C3444;
};

inline constexpr AudioConfig audio{};
inline constexpr UiConfig ui{};

inline float waveformPeak(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine: return audio.sine_peak;
    case Waveform::Saw: return audio.saw_peak;
    case Waveform::Square: return audio.square_peak;
    case Waveform::Triangle: return audio.triangle_peak;
    default: return audio.sine_peak;
  }
}

inline float waveformTrim(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine: return audio.sine_trim;
    case Waveform::Saw: return audio.saw_trim;
    case Waveform::Square: return audio.square_trim;
    case Waveform::Triangle: return audio.triangle_trim;
    default: return audio.sine_trim;
  }
}

}  // namespace SynthConfig
