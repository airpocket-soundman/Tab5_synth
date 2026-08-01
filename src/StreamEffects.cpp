#include "StreamEffects.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kTwoPi = 6.28318530718f;
// Match AudioEngine's note-based delay range.
constexpr float kDelayMinMs = 40.0f;
constexpr float kDelayMaxMs = 700.0f;
constexpr float kChorusBaseMs = 8.0f;
constexpr float kChorusDepthMaxMs = 12.0f;
}  // namespace

float StreamEffects::clampUnit(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

void StreamEffects::begin(float sample_rate) {
  sample_rate_ = sample_rate;
  reset();
}

void StreamEffects::reset() {
  delay_line_.fill(0);
  delay_pos_ = 0;
  chorus_line_.fill(0);
  chorus_pos_ = 0;
  chorus_phase_ = 0.0f;
  filter_low_ = 0.0f;
  filter_band_ = 0.0f;
  tone_state_ = 0.0f;
  crush_hold_ = 0.0f;
  crush_counter_ = 0.0f;
}

void StreamEffects::setDelayParameters(float time_normalized, float feedback_normalized, float mix_normalized) {
  delay_time_normalized_ = clampUnit(time_normalized);
  delay_feedback_normalized_ = clampUnit(feedback_normalized);
  delay_mix_normalized_ = clampUnit(mix_normalized);
}

void StreamEffects::setChorusParameters(float rate_normalized, float depth_normalized, float mix_normalized) {
  chorus_rate_normalized_ = clampUnit(rate_normalized);
  chorus_depth_normalized_ = clampUnit(depth_normalized);
  chorus_mix_normalized_ = clampUnit(mix_normalized);
}

void StreamEffects::setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized) {
  filter_cutoff_normalized_ = clampUnit(cutoff_normalized);
  filter_resonance_normalized_ = clampUnit(resonance_normalized);
  filter_mix_normalized_ = clampUnit(mix_normalized);
}

void StreamEffects::setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized) {
  distortion_drive_normalized_ = clampUnit(drive_normalized);
  distortion_tone_normalized_ = clampUnit(tone_normalized);
  distortion_mix_normalized_ = clampUnit(mix_normalized);
}

void StreamEffects::setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized) {
  bitcrusher_bits_normalized_ = clampUnit(bits_normalized);
  bitcrusher_rate_normalized_ = clampUnit(rate_normalized);
  bitcrusher_mix_normalized_ = clampUnit(mix_normalized);
}

void StreamEffects::process(std::int16_t* samples, std::size_t count) {
  // Snapshot parameters and precompute per-chunk coefficients so UI writes
  // mid-chunk cannot tear the DSP math.
  const bool dist_on = distortion_enabled_ && distortion_mix_normalized_ > 0.0f;
  const float dist_mix = distortion_mix_normalized_;
  const float dist_drive = 1.0f + distortion_drive_normalized_ * 24.0f;
  // Tone: one-pole lowpass 500Hz..7kHz.
  const float tone_hz = 500.0f * std::pow(14.0f, distortion_tone_normalized_);
  const float tone_coef = std::clamp(1.0f - std::exp(-kTwoPi * tone_hz / sample_rate_), 0.0f, 1.0f);

  const bool crush_on = bitcrusher_enabled_ && bitcrusher_mix_normalized_ > 0.0f;
  const float crush_mix = bitcrusher_mix_normalized_;
  const float crush_bits = 2.0f + bitcrusher_bits_normalized_ * 14.0f;
  const float crush_levels = std::pow(2.0f, crush_bits) * 0.5f;
  const float crush_step = 1.0f / (1.0f + (1.0f - bitcrusher_rate_normalized_) * 49.0f);

  const bool filter_on = filter_enabled_ && filter_mix_normalized_ > 0.0f;
  const float filter_mix = filter_mix_normalized_;
  const float cutoff_hz = 120.0f * std::pow(66.0f, filter_cutoff_normalized_);
  const float f_coef = std::clamp(2.0f * std::sin(3.14159265f * cutoff_hz / sample_rate_), 0.0f, 1.2f);
  const float damp = 1.0f / (0.7f + filter_resonance_normalized_ * 7.0f);

  const bool chorus_on = chorus_enabled_ && chorus_mix_normalized_ > 0.0f;
  const float chorus_mix = chorus_mix_normalized_;
  const float chorus_rate_hz = 0.1f + chorus_rate_normalized_ * 4.9f;
  const float chorus_phase_step = kTwoPi * chorus_rate_hz / sample_rate_;
  const float chorus_base = kChorusBaseMs * 0.001f * sample_rate_;
  const float chorus_depth = chorus_depth_normalized_ * kChorusDepthMaxMs * 0.001f * sample_rate_;

  const bool delay_on = delay_enabled_ && delay_mix_normalized_ > 0.0f;
  const float delay_mix = delay_mix_normalized_;
  // Square-root curve so mid knob positions give keyboard-like repeat counts
  // (linear mapping died after 2-3 echoes at typical settings). Cap 0.96:
  // at max FBK each echo loses only ~0.35dB, so the tail rings 15-20s
  // before the stream's silence watchdog retires it; 1.0 would never decay.
  const float delay_feedback = std::sqrt(delay_feedback_normalized_) * 0.96f;
  const float delay_ms = kDelayMinMs + (kDelayMaxMs - kDelayMinMs) * delay_time_normalized_;
  const std::size_t delay_samples =
      std::clamp<std::size_t>(static_cast<std::size_t>(delay_ms * 0.001f * sample_rate_), 1, kMaxDelaySamples - 1);

  for (std::size_t i = 0; i < count; ++i) {
    float x = static_cast<float>(samples[i]) / 32768.0f;

    if (dist_on) {
      float d = std::tanh(x * dist_drive);
      tone_state_ += tone_coef * (d - tone_state_);
      d = tone_state_;
      x += (d - x) * dist_mix;
    }

    if (crush_on) {
      crush_counter_ += crush_step;
      if (crush_counter_ >= 1.0f) {
        crush_counter_ -= 1.0f;
        crush_hold_ = std::round(x * crush_levels) / crush_levels;
      }
      x += (crush_hold_ - x) * crush_mix;
    }

    if (filter_on) {
      filter_low_ += f_coef * filter_band_;
      const float high = x - filter_low_ - damp * filter_band_;
      filter_band_ += f_coef * high;
      filter_low_ = std::clamp(filter_low_, -2.0f, 2.0f);
      filter_band_ = std::clamp(filter_band_, -2.0f, 2.0f);
      x += (filter_low_ - x) * filter_mix;
    }

    if (chorus_on) {
      chorus_line_[chorus_pos_] = static_cast<std::int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f);
      chorus_phase_ += chorus_phase_step;
      if (chorus_phase_ >= kTwoPi) {
        chorus_phase_ -= kTwoPi;
      }
      const float lfo = 0.5f + 0.5f * std::sin(chorus_phase_);
      const float tap = chorus_base + chorus_depth * lfo;
      const float read_pos = static_cast<float>(chorus_pos_) + static_cast<float>(kChorusSamples) - tap;
      const std::size_t idx0 = static_cast<std::size_t>(read_pos) % kChorusSamples;
      const std::size_t idx1 = (idx0 + 1) % kChorusSamples;
      const float frac = read_pos - std::floor(read_pos);
      const float wet = (static_cast<float>(chorus_line_[idx0]) * (1.0f - frac) +
                         static_cast<float>(chorus_line_[idx1]) * frac) /
                        32768.0f;
      chorus_pos_ = (chorus_pos_ + 1) % kChorusSamples;
      x += (wet - x) * chorus_mix * 0.5f;
    }

    if (delay_on) {
      const std::size_t read_index = (delay_pos_ + kMaxDelaySamples - delay_samples) % kMaxDelaySamples;
      const float wet = static_cast<float>(delay_line_[read_index]) / 32768.0f;
      const float fb_in = std::clamp(x + wet * delay_feedback, -1.0f, 1.0f);
      delay_line_[delay_pos_] = static_cast<std::int16_t>(fb_in * 32767.0f);
      delay_pos_ = (delay_pos_ + 1) % kMaxDelaySamples;
      x += wet * delay_mix;
    }

    samples[i] = static_cast<std::int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f);
  }
}
