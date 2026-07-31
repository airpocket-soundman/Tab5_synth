#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Per-sample DSP chain for the external I2S stream:
// distortion -> bitcrusher -> filter -> chorus -> delay.
// Parameters use the same normalized [0..1] ranges as AudioEngine so the
// existing UI controls drive both the oscillator and the stream path.
class StreamEffects {
 public:
  void begin(float sample_rate);
  void reset();
  void process(std::int16_t* samples, std::size_t count);

  void setDelayEnabled(bool enabled) { delay_enabled_ = enabled; }
  void setDelayParameters(float time_normalized, float feedback_normalized, float mix_normalized);
  void setChorusEnabled(bool enabled) { chorus_enabled_ = enabled; }
  void setChorusParameters(float rate_normalized, float depth_normalized, float mix_normalized);
  void setFilterEnabled(bool enabled) { filter_enabled_ = enabled; }
  void setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized);
  void setDistortionEnabled(bool enabled) { distortion_enabled_ = enabled; }
  void setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized);
  void setBitcrusherEnabled(bool enabled) { bitcrusher_enabled_ = enabled; }
  void setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized);

 private:
  static float clampUnit(float value);

  static constexpr std::size_t kMaxDelaySamples = 11264;  // 704ms @ 16kHz
  static constexpr std::size_t kChorusSamples = 512;      // 32ms @ 16kHz

  float sample_rate_ = 16000.0f;

  bool delay_enabled_ = false;
  float delay_time_normalized_ = 0.35f;
  float delay_feedback_normalized_ = 0.40f;
  float delay_mix_normalized_ = 0.0f;

  bool chorus_enabled_ = false;
  float chorus_rate_normalized_ = 0.30f;
  float chorus_depth_normalized_ = 0.40f;
  float chorus_mix_normalized_ = 0.0f;

  bool filter_enabled_ = false;
  float filter_cutoff_normalized_ = 1.0f;
  float filter_resonance_normalized_ = 0.25f;
  float filter_mix_normalized_ = 0.0f;

  bool distortion_enabled_ = false;
  float distortion_drive_normalized_ = 0.40f;
  float distortion_tone_normalized_ = 0.55f;
  float distortion_mix_normalized_ = 0.0f;

  bool bitcrusher_enabled_ = false;
  float bitcrusher_bits_normalized_ = 1.0f;
  float bitcrusher_rate_normalized_ = 1.0f;
  float bitcrusher_mix_normalized_ = 0.0f;

  // DSP state
  std::array<std::int16_t, kMaxDelaySamples> delay_line_{};
  std::size_t delay_pos_ = 0;
  std::array<std::int16_t, kChorusSamples> chorus_line_{};
  std::size_t chorus_pos_ = 0;
  float chorus_phase_ = 0.0f;
  float filter_low_ = 0.0f;
  float filter_band_ = 0.0f;
  float tone_state_ = 0.0f;
  float crush_hold_ = 0.0f;
  float crush_counter_ = 0.0f;
};
