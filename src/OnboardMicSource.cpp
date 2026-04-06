#include "OnboardMicSource.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr float kNormalizeTarget = 0.82f;
constexpr std::size_t kFadeSamples = 128;

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
  setVolume(volume_);
  return initialized_;
}

bool OnboardMicSource::noteOn(std::size_t voice_index, float note_value, float /*frequency*/, Waveform /*waveform*/) {
  if (!sample_buffer_.hasData() || recording_) {
    return false;
  }

  const float ratio = noteToRatio(note_value);
  const std::uint32_t playback_rate = static_cast<std::uint32_t>(std::lround(
      static_cast<float>(SynthConfig::audio.mic_sample_rate) * ratio));
  setVoiceLevel(voice_index, volume_, Waveform::Sine);
  return M5.Speaker.playRaw(sample_buffer_.data(), sample_buffer_.lengthSamples(), playback_rate, false,
                            UINT32_MAX, channelForVoice(voice_index), true);
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

  if (commit_sample && recording_buffer_.hasData()) {
    const std::size_t processed_length = preprocessSample(recording_buffer_, sample_buffer_);
    sample_buffer_.setLengthSamples(processed_length);
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
  return sample_buffer_.allocate(maxRecordSamples()) && recording_buffer_.allocate(maxRecordSamples());
}

float OnboardMicSource::noteToRatio(float note_value) {
  return std::pow(2.0f, (note_value - static_cast<float>(SynthConfig::audio.mic_sample_root_note)) / 12.0f);
}

int OnboardMicSource::channelForVoice(std::size_t voice_index) const {
  return SynthConfig::audio.audio_channel + static_cast<int>(voice_index);
}
