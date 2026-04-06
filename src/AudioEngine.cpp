#include "AudioEngine.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>

void AudioEngine::begin() {
  M5.Speaker.setVolume(SynthConfig::audio.speaker_volume);
  clearVoices();
  oscillator_source_.begin();
  onboard_mic_source_.begin();
  external_i2s_source_.begin();
  oscillator_source_.setVolume(volume_);
  onboard_mic_source_.setVolume(volume_);
  external_i2s_source_.setVolume(volume_);
}

void AudioEngine::noteOnVoices(const float* note_values, std::size_t count, Waveform waveform) {
  AudioSource& source = sourceFor(active_source_type_);
  if (!source.isAvailable() || count == 0) {
    noteOff();
    return;
  }

  const std::size_t clamped_count = std::min(count, SynthConfig::audio.polyphony_voices);

  if (active_source_type_ == AudioSourceType::ExternalI2S) {
    const float frequency = noteValueToFrequency(note_values[0]);
    if (!source.noteOn(0, note_values[0], frequency, waveform)) {
      noteOff();
      return;
    }
    clearVoices();
    voice_active_[0] = true;
    active_midi_notes_[0] = static_cast<int>(std::lround(note_values[0]));
    active_frequencies_[0] = frequency;
    active_waveform_ = waveform;
    return;
  }

  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (i < clamped_count) {
      const float frequency = noteValueToFrequency(note_values[i]);
      const bool retrigger_required = active_source_type_ != AudioSourceType::OnboardMic ||
                                      !voice_active_[i] ||
                                      std::fabs(active_note_values_[i] - note_values[i]) >= 0.05f;
      if (!retrigger_required) {
        active_midi_notes_[i] = static_cast<int>(std::lround(note_values[i]));
        active_note_values_[i] = note_values[i];
        active_frequencies_[i] = frequency;
        continue;
      }

      if (source.noteOn(i, note_values[i], frequency, waveform)) {
        voice_active_[i] = true;
        active_midi_notes_[i] = static_cast<int>(std::lround(note_values[i]));
        active_note_values_[i] = note_values[i];
        active_frequencies_[i] = frequency;
      } else {
        voice_active_[i] = false;
        active_midi_notes_[i] = -1;
        active_note_values_[i] = 0.0f;
        active_frequencies_[i] = 0.0f;
      }
    } else if (voice_active_[i]) {
      source.noteOff(i);
      voice_active_[i] = false;
      active_midi_notes_[i] = -1;
      active_note_values_[i] = 0.0f;
      active_frequencies_[i] = 0.0f;
    }
  }

  active_waveform_ = waveform;
}

void AudioEngine::noteOff() {
  sourceFor(active_source_type_).noteOffAll();
  clearVoices();
}

void AudioEngine::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  oscillator_source_.setVolume(volume_);
  onboard_mic_source_.setVolume(volume_);
  external_i2s_source_.setVolume(volume_);
}

void AudioEngine::setSourceType(AudioSourceType source_type) {
  if (active_source_type_ == source_type) {
    return;
  }

  noteOff();
  active_source_type_ = source_type;
}

bool AudioEngine::beginMicSampleRecording() {
  noteOff();
  const bool started = onboard_mic_source_.beginRecording();
  if (started) {
    active_source_type_ = AudioSourceType::OnboardMic;
  }
  return started;
}

void AudioEngine::updateMicSampleRecording() {
  onboard_mic_source_.updateRecording();
}

bool AudioEngine::finishMicSampleRecording(bool commit_sample) {
  const bool recorded = onboard_mic_source_.finishRecording(commit_sample);
  if (recorded) {
    active_source_type_ = AudioSourceType::OnboardMic;
  }
  return recorded;
}

bool AudioEngine::recordMicSample() {
  noteOff();
  const bool recorded = onboard_mic_source_.recordSample();
  if (recorded) {
    active_source_type_ = AudioSourceType::OnboardMic;
  }
  return recorded;
}

bool AudioEngine::hasMicSample() const {
  return onboard_mic_source_.hasSample();
}

bool AudioEngine::isNotePlaying() const {
  return activeVoiceCount() > 0;
}

bool AudioEngine::isCurrentSourceAvailable() const {
  return sourceFor(active_source_type_).isAvailable();
}

bool AudioEngine::isSourceAvailable(AudioSourceType source_type) const {
  return sourceFor(source_type).isAvailable();
}

int AudioEngine::activeMidiNote() const {
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i]) {
      return active_midi_notes_[i];
    }
  }
  return -1;
}

float AudioEngine::activeFrequency() const {
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i]) {
      return active_frequencies_[i];
    }
  }
  return 0.0f;
}

Waveform AudioEngine::activeWaveform() const {
  return active_waveform_;
}

bool AudioEngine::isMicRecording() const {
  return onboard_mic_source_.isRecording();
}

float AudioEngine::volume() const {
  return volume_;
}

AudioSourceType AudioEngine::activeSourceType() const {
  return active_source_type_;
}

std::size_t AudioEngine::activeVoiceCount() const {
  std::size_t count = 0;
  for (bool active : voice_active_) {
    if (active) {
      ++count;
    }
  }
  return count;
}

float AudioEngine::noteValueToFrequency(float note_value) {
  return 440.0f * std::pow(2.0f, (note_value - 69.0f) / 12.0f);
}

AudioSource& AudioEngine::sourceFor(AudioSourceType source_type) {
  switch (source_type) {
    case AudioSourceType::Oscillator:
      return oscillator_source_;
    case AudioSourceType::OnboardMic:
      return onboard_mic_source_;
    case AudioSourceType::ExternalI2S:
      return external_i2s_source_;
    default:
      return oscillator_source_;
  }
}

const AudioSource& AudioEngine::sourceFor(AudioSourceType source_type) const {
  switch (source_type) {
    case AudioSourceType::Oscillator:
      return oscillator_source_;
    case AudioSourceType::OnboardMic:
      return onboard_mic_source_;
    case AudioSourceType::ExternalI2S:
      return external_i2s_source_;
    default:
      return oscillator_source_;
  }
}

void AudioEngine::clearVoices() {
  voice_active_.fill(false);
  active_midi_notes_.fill(-1);
  active_frequencies_.fill(0.0f);
}





