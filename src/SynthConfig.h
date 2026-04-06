#pragma once

#include "Waveform.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace SynthConfig {

struct AudioConfig {
  int speaker_volume = 255;
  int audio_channel = 0;
  float default_volume = 0.7f;
  float center_sample = 128.0f;
  std::size_t wavetable_size = 64;
  float sine_peak = 127.0f;
  float saw_peak = 46.0f;
  float square_peak = 34.0f;
  float triangle_peak = 127.0f;
  float sine_trim = 1.20f;
  float saw_trim = 0.88f;
  float square_trim = 0.82f;
  float triangle_trim = 0.94f;
};

struct UiConfig {
  // 波形ボタンを横に並べる列数。
  int wave_button_columns = 4;
  // 波形ボタン同士の縦横の間隔。
  int wave_button_gap = 14;
  // 波形ボタン 1 個あたりの高さ。
  int wave_button_height = 92;
  // 左カラム外周の余白。
  int wave_button_padding = 20;
  // 波形ボタン幅の倍率。1.0 で列幅いっぱい。
  float wave_button_width_scale = 1.0f;
  // 波形アイコンの線の太さ。
  int wave_icon_thickness = 3;
  // 波形アイコン下に付けるラベル文字サイズ。
  int wave_label_text_size = 1;
  // 波形ラベルを表示するか。
  bool show_wave_labels = false;
  // ボリューム領域の高さ。
  int volume_area_height = 74;
  // ボリュームスライダーのトラックの太さ。
  int slider_track_height = 18;
  // ボリュームスライダー左右の内側余白。
  int slider_inset = 18;
  // ボリューム数値の右側余白。
  int volume_value_margin_right = 18;
  // ボリューム数値とスライダーの間隔。
  int volume_value_gap = 12;
  // ボリューム数値の表示幅。
  int volume_value_width = 48;
  // 演奏領域を画面幅の何割にするか。0.5 なら右半分。
  float performance_area_width_ratio = 0.5f;
  // タッチ位置表示の左余白。
  int coordinate_text_margin_left = 16;
  // タッチ位置表示の下余白。
  int coordinate_text_margin_bottom = 14;
  // タッチ位置表示の文字サイズ。
  int coordinate_text_size = 1;
  // タッチ位置表示のクリア領域の幅。
  int coordinate_text_width = 260;
  // タッチ位置表示のクリア領域の高さ。
  int coordinate_text_height = 20;
  // タッチマーカー領域の左右余白。
  int marker_side_margin = 10;
  // タッチマーカー領域の下余白。
  int marker_bottom_margin = 44;
  // タッチマーカー領域を上側から始める最低 Y 座標。
  int marker_min_top = 120;
  // x=0, y=0 ガイド線の点線 1 セグメントの長さ。
  int center_guide_segment = 8;
  // x=0, y=0 ガイド線の点線の隙間長。
  int center_guide_gap = 6;
  // 演奏領域の左端に割り当てる MIDI ノート番号。
  int min_midi_note = 48;
  // 演奏領域の右端に割り当てる MIDI ノート番号。高くすると高音まで出せる。
  int max_midi_note = 96;
  // 波形ボタンやボリューム枠の角丸半径。
  int round_radius = 16;
  // タッチ位置マーカーの半径。
  int marker_radius = 18;
  // 画面全体の背景色。
  std::uint32_t background_color = 0x05070C;
  // 右側演奏領域の背景色。
  std::uint32_t panel_color = 0x0B1020;
  // パネルや枠線に使う境界色。
  std::uint32_t panel_border_color = 0x394867;
  // 非選択ボタンの背景色。
  std::uint32_t muted_button_color = 0x1B1B1B;
  // 非選択ボタンの枠線色。
  std::uint32_t muted_border_color = 0x555555;
  // 通常テキスト色。
  std::uint32_t muted_text_color = 0xEAEAEA;
  // 選択中ボタンの枠線や強調色。
  std::uint32_t selected_border_color = 0xF4F1DE;
  // 選択中ボタン上の文字色。
  std::uint32_t selected_text_color = 0x081018;
  // ボリュームスライダーの充填色。
  std::uint32_t slider_fill_color = 0x4CC9F0;
  // ボリュームスライダーのベース色。
  std::uint32_t slider_track_color = 0x162033;
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
