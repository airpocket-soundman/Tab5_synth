#include "OnboardMicSource.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr float kNormalizeTarget = 0.82f;
constexpr std::size_t kFadeSamples = 128;
constexpr float kIdentityPitchTolerance = 0.01f;

std::size_t maxRecordSamples() {
  return (static_cast<std::size_t>(SynthConfig::audio.mic_sample_rate) *
          static_cast<std::size_t>(SynthConfig::audio.mic_sample_max_ms)) /
         1000;
}

std::size_t findStartOffset(const std::int16_t* data, std::size_t length) {
  for (std::size_t i = 0; i < length; ++i) {
    if (std::abs(static_cast<int>(data[i])) >= SynthConfig::audio.mic_trim_threshold) {
      return i;
    }
  }
  return length;
}

std::size_t findEndOffset(const std::int16_t* data, std::size_t length) {
  for (std::size_t i = length; i > 0; --i) {
    if (std::abs(static_cast<int>(data[i - 1])) >= SynthConfig::audio.mic_trim_threshold) {
      return i;
    }
  }
  return 0;
}

std::size_t preprocessSample(const SampleBuffer& source, SampleBuffer& dest) {
  if (!source.hasData() || !dest.isAllocated()) {
    return 0;
  }

  const auto* input = source.data();
  const std::size_t input_length = source.lengthSamples();
  const std::size_t start = findStartOffset(input, input_length);
  const std::size_t end = findEndOffset(input, input_length);
  if (start >= end) {
    return 0;
  }

  const std::size_t trimmed_length = std::min(end - start, dest.capacitySamples());
  auto* output = dest.data();

  std::int64_t sum = 0;
  for (std::size_t i = 0; i < trimmed_length; ++i) {
    sum += input[start + i];
  }
  const float dc_offset = static_cast<float>(sum) / static_cast<float>(trimmed_length);

  float peak = 1.0f;
  for (std::size_t i = 0; i < trimmed_length; ++i) {
    const float centered = static_cast<float>(input[start + i]) - dc_offset;
    peak = std::max(peak, std::fabs(centered));
  }

  const float gain = (32767.0f * kNormalizeTarget) / peak;
  const std::size_t fade = std::min(kFadeSamples, trimmed_length / 2);

  for (std::size_t i = 0; i < trimmed_length; ++i) {
    float sample = (static_cast<float>(input[start + i]) - dc_offset) * gain;
    if (fade > 0) {
      if (i < fade) {
        sample *= static_cast<float>(i) / static_cast<float>(fade);
      } else if (i >= trimmed_length - fade) {
        sample *= static_cast<float>(trimmed_length - 1 - i) / static_cast<float>(fade);
      }
    }
    output[i] = static_cast<std::int16_t>(std::clamp(sample, -32767.0f, 32767.0f));
  }

  return trimmed_length;
}

}  // namespace

bool OnboardMicSource::begin() {
  (void)AudioBusConfig::OnboardMicPins::mclk;
  (void)AudioBusConfig::OnboardMicPins::bclk;
  (void)AudioBusConfig::OnboardMicPins::data;
  (void)AudioBusConfig::OnboardMicPins::ws;
  (void)AudioBusConfig::OnboardMicPins::i2c_scl;
  (void)AudioBusConfig::OnboardMicPins::i2c_sda;
  initialized_ = ensureBuffer();
  pitched_cache_valid_.fill(false);
  pitched_cache_note_.fill(0.0f);
  pitched_cache_generation_.fill(0);
  setVolume(volume_);
  return initialized_;
}

bool OnboardMicSource::noteOn(std::size_t voice_index, float note_value, float /*frequency*/, Waveform /*waveform*/) {
  if (!sample_buffer_.hasData() || recording_) {
    return false;
  }

  const float ratio = noteToRatio(note_value);
  const std::uint32_t playback_rate = static_cast<std::uint32_t>(
      std::lround(static_cast<float>(SynthConfig::audio.mic_sample_rate) * std::clamp(ratio, 0.125f, 8.0f)));

  setVoiceLevel(voice_index, volume_, Waveform::Sine);
  return M5.Speaker.playRaw(sample_buffer_.data(), sample_buffer_.lengthSamples(), playback_rate, false,
                            1, channelForVoice(voice_index), true);
}

void OnboardMicSource::noteOff(std::size_t voice_index) {
  M5.Speaker.stop(channelForVoice(voice_index));
}

void OnboardMicSource::noteOffAll() {
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    noteOff(i);
  }
}

void OnboardMicSource::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void OnboardMicSource::setVoiceLevel(std::size_t voice_index, float level, Waveform /*waveform*/) {
  const float scaled = std::clamp(level, 0.0f, 1.0f);
  M5.Speaker.setChannelVolume(channelForVoice(voice_index),
                              static_cast<std::uint8_t>(std::lround(scaled * 255.0f)));
}

bool OnboardMicSource::isAvailable() const {
  return initialized_;
}

AudioSourceType OnboardMicSource::type() const {
  return AudioSourceType::OnboardMic;
}

bool OnboardMicSource::beginRecording() {
  if (!ensureBuffer()) {
    initialized_ = false;
    return false;
  }

  if (recording_) {
    return true;
  }

  has_backup_sample_ = false;
  if (sample_buffer_.hasData() && backup_sample_buffer_.isAllocated() &&
      backup_sample_buffer_.capacitySamples() >= sample_buffer_.lengthSamples()) {
    const std::size_t backup_length = sample_buffer_.lengthSamples();
    std::memcpy(backup_sample_buffer_.data(), sample_buffer_.data(), backup_length * sizeof(std::int16_t));
    backup_sample_buffer_.setLengthSamples(backup_length);
    has_backup_sample_ = true;
  }

  recording_buffer_.clear();
  recorded_samples_ = 0;
  noteOffAll();
  M5.Speaker.end();

  if (!M5.Mic.begin()) {
    M5.Speaker.begin();
    setVolume(volume_);
    return false;
  }

  initialized_ = true;
  recording_ = true;
  return true;
}

void OnboardMicSource::updateRecording() {
  if (!recording_) {
    return;
  }

  if (recorded_samples_ >= recording_buffer_.capacitySamples()) {
    return;
  }

  const std::size_t chunk = std::min<std::size_t>(SynthConfig::audio.mic_record_block_samples,
                                                  recording_buffer_.capacitySamples() - recorded_samples_);
  if (M5.Mic.record(recording_buffer_.data() + recorded_samples_, chunk, SynthConfig::audio.mic_sample_rate, true)) {
    recorded_samples_ += chunk;
    recording_buffer_.setLengthSamples(recorded_samples_);
  }
}

bool OnboardMicSource::finishRecording(bool commit_sample) {
  if (!recording_) {
    return sample_buffer_.hasData();
  }

  M5.Mic.end();
  M5.Speaker.begin();
  setVolume(volume_);
  recording_ = false;
  recording_buffer_.setLengthSamples(recorded_samples_);

  bool committed = false;
  if (commit_sample && recording_buffer_.hasData()) {
    const std::size_t processed_length = preprocessSample(recording_buffer_, sample_buffer_);
    sample_buffer_.setLengthSamples(processed_length);
    committed = processed_length > 0;
    if (committed) {
      ++sample_generation_;
      pitched_cache_valid_.fill(false);
    }
  }

  if (!committed && has_backup_sample_ && backup_sample_buffer_.hasData() &&
      sample_buffer_.isAllocated() &&
      sample_buffer_.capacitySamples() >= backup_sample_buffer_.lengthSamples()) {
    const std::size_t restore_length = backup_sample_buffer_.lengthSamples();
    std::memcpy(sample_buffer_.data(), backup_sample_buffer_.data(), restore_length * sizeof(std::int16_t));
    sample_buffer_.setLengthSamples(restore_length);
    ++sample_generation_;
    pitched_cache_valid_.fill(false);
  }

  return sample_buffer_.hasData();
}

bool OnboardMicSource::recordSample() {
  if (!beginRecording()) {
    return false;
  }

  while (recording_ && recorded_samples_ < recording_buffer_.capacitySamples()) {
    updateRecording();
    M5.delay(1);
  }

  return finishRecording(true);
}

bool OnboardMicSource::hasSample() const {
  return sample_buffer_.hasData();
}

bool OnboardMicSource::isRecording() const {
  return recording_;
}

bool OnboardMicSource::ensureBuffer() {
  const std::size_t required = maxRecordSamples();
  if ((!sample_buffer_.isAllocated() || sample_buffer_.capacitySamples() < required) &&
      !sample_buffer_.allocate(required)) {
    return false;
  }
  if ((!backup_sample_buffer_.isAllocated() || backup_sample_buffer_.capacitySamples() < required) &&
      !backup_sample_buffer_.allocate(required)) {
    return false;
  }
  if ((!recording_buffer_.isAllocated() || recording_buffer_.capacitySamples() < required) &&
      !recording_buffer_.allocate(required)) {
    return false;
  }
  for (auto& buffer : pitched_buffers_) {
    if ((!buffer.isAllocated() || buffer.capacitySamples() < required) && !buffer.allocate(required)) {
      return false;
    }
  }
  return true;
}

bool OnboardMicSource::renderPitchShiftedSample(float note_value, SampleBuffer& destination) const {
  if (!sample_buffer_.hasData() || !destination.isAllocated()) {
    return false;
  }

  const auto* input = sample_buffer_.data();
  const std::size_t input_length = sample_buffer_.lengthSamples();
  if (input_length < 8) {
    return false;
  }

  auto* output = destination.data();
  if (output == nullptr) {
    return false;
  }

  const float ratio = std::clamp(noteToRatio(note_value), 0.125f, 8.0f);
  if (std::fabs(ratio - 1.0f) < kIdentityPitchTolerance) {
    std::memcpy(output, input, input_length * sizeof(std::int16_t));
    destination.setLengthSamples(input_length);
    return true;
  }

  static thread_local std::vector<float> accum;
  static thread_local std::vector<float> norm;
  accum.assign(input_length, 0.0f);
  norm.assign(input_length, 0.0f);

  const std::size_t grain_size = std::min<std::size_t>(256, std::max<std::size_t>(96, input_length / 16));
  const std::size_t hop_out = std::max<std::size_t>(48, grain_size / 2);
  const int half = static_cast<int>(grain_size / 2);
  constexpr float kTwoPi = 6.28318530718f;

  for (std::size_t center_out = 0; center_out < input_length; center_out += hop_out) {
    const float center_src = static_cast<float>(center_out) * ratio;
    for (int k = -half; k < half; ++k) {
      const int out_i = static_cast<int>(center_out) + k;
      if (out_i < 0 || out_i >= static_cast<int>(input_length)) {
        continue;
      }

      const float phase = static_cast<float>(k + half) / static_cast<float>(grain_size - 1);
      const float window = 0.5f - 0.5f * std::cos(kTwoPi * phase);
      const float src_pos = center_src + static_cast<float>(k) * ratio;
      const float sample = sampleAtCubic(input, input_length, src_pos);

      accum[static_cast<std::size_t>(out_i)] += sample * window;
      norm[static_cast<std::size_t>(out_i)] += window;
    }
  }

  for (std::size_t i = 0; i < input_length; ++i) {
    float sample = (norm[i] > 1e-6f) ? (accum[i] / norm[i]) : 0.0f;
    const std::size_t fade = std::min<std::size_t>(kFadeSamples, input_length / 2);
    if (fade > 0) {
      if (i < fade) {
        sample *= static_cast<float>(i) / static_cast<float>(fade);
      } else if (i >= input_length - fade) {
        sample *= static_cast<float>(input_length - 1 - i) / static_cast<float>(fade);
      }
    }
    output[i] = static_cast<std::int16_t>(std::clamp(sample, -32767.0f, 32767.0f));
  }

  destination.setLengthSamples(input_length);
  return true;
}

float OnboardMicSource::sampleAtCubic(const std::int16_t* data, std::size_t length, float index) {
  if (data == nullptr || length == 0) {
    return 0.0f;
  }

  const float clamped = std::clamp(index, 0.0f, static_cast<float>(length - 1));
  const int i1 = static_cast<int>(clamped);
  const int i0 = std::max(0, i1 - 1);
  const int i2 = std::min(static_cast<int>(length - 1), i1 + 1);
  const int i3 = std::min(static_cast<int>(length - 1), i1 + 2);
  const float t = clamped - static_cast<float>(i1);

  const float p0 = static_cast<float>(data[i0]);
  const float p1 = static_cast<float>(data[i1]);
  const float p2 = static_cast<float>(data[i2]);
  const float p3 = static_cast<float>(data[i3]);

  const float a = (-0.5f * p0) + (1.5f * p1) - (1.5f * p2) + (0.5f * p3);
  const float b = p0 - (2.5f * p1) + (2.0f * p2) - (0.5f * p3);
  const float c = (-0.5f * p0) + (0.5f * p2);
  const float d = p1;
  return ((a * t + b) * t + c) * t + d;
}

float OnboardMicSource::sampleAtBandlimited(const std::int16_t* data,
                                            std::size_t length,
                                            float index,
                                            float ratio) {
  if (ratio <= 1.0f) {
    return sampleAtCubic(data, length, index);
  }

  const int taps = std::clamp(static_cast<int>(std::ceil(ratio * 4.0f)), 4, 32);
  const float width = ratio;
  float sum = 0.0f;
  for (int i = 0; i < taps; ++i) {
    const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(taps);
    const float offset = (t - 0.5f) * width;
    sum += sampleAtCubic(data, length, index + offset);
  }
  return sum / static_cast<float>(taps);
}

float OnboardMicSource::noteToRatio(float note_value) {
  return std::pow(2.0f, (note_value - static_cast<float>(SynthConfig::audio.mic_sample_root_note)) / 12.0f);
}

int OnboardMicSource::channelForVoice(std::size_t voice_index) const {
  return SynthConfig::audio.audio_channel + static_cast<int>(voice_index);
}
