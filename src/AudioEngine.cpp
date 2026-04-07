#include "AudioEngine.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPitchRetuneThresholdSemitone = 0.25f;
constexpr std::uint32_t kMinRetuneIntervalMs = 24;
}

void AudioEngine::begin() {
  M5.Speaker.setVolume(SynthConfig::audio.speaker_volume);
  amp_envelope_.attack_seconds = SynthConfig::audio.amp_attack_default_ms / 1000.0f;
  amp_envelope_.decay_seconds = SynthConfig::audio.amp_decay_default_ms / 1000.0f;
  amp_envelope_.sustain_level = SynthConfig::audio.amp_sustain_default;
  amp_envelope_.release_seconds = SynthConfig::audio.amp_release_default_ms / 1000.0f;
  clearVoices();
  applyEnvelopeSettings();
  oscillator_source_.begin();
  onboard_mic_source_.begin();
  external_i2s_source_.begin();
  oscillator_source_.setVolume(volume_);
  onboard_mic_source_.setVolume(volume_);
  external_i2s_source_.setVolume(volume_);
  last_envelope_update_ms_ = millis();
}

void AudioEngine::update() {
  const std::uint32_t now = millis();
  const float delta_seconds = std::min(0.05f, static_cast<float>(now - last_envelope_update_ms_) / 1000.0f);
  last_envelope_update_ms_ = now;

  AudioSource& source = sourceFor(active_source_type_);
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (!voice_active_[i]) {
      continue;
    }

    const float env = envelopes_[i].process(delta_seconds);
    if (!envelopes_[i].isActive()) {
      source.setVoiceLevel(i, 0.0f, active_waveform_);
      source.noteOff(i);
      voice_active_[i] = false;
      voice_held_[i] = false;
      active_midi_notes_[i] = -1;
      active_note_values_[i] = 0.0f;
      active_frequencies_[i] = 0.0f;
      last_retrigger_ms_[i] = 0;
      voice_started_order_[i] = 0;
      continue;
    }

    source.setVoiceLevel(i, volume_ * env, active_waveform_);
  }
}

void AudioEngine::noteOnVoices(const float* note_values, std::size_t count, Waveform waveform) {
  AudioSource& source = sourceFor(active_source_type_);
  if (!source.isAvailable() || count == 0) {
    noteOff();
    return;
  }

  constexpr std::size_t npos = static_cast<std::size_t>(-1);
  std::array<float, SynthConfig::audio.polyphony_voices> requested_note_values{};
  std::array<int, SynthConfig::audio.polyphony_voices> requested_midi_notes{};
  std::size_t requested_count = 0;
  const std::size_t input_count = std::min(count, SynthConfig::audio.polyphony_voices);
  for (std::size_t i = 0; i < input_count; ++i) {
    const float note_value = note_values[i];
    const int midi_note = static_cast<int>(std::lround(note_value));
    bool already_requested = false;
    for (std::size_t j = 0; j < requested_count; ++j) {
      if (requested_midi_notes[j] == midi_note) {
        already_requested = true;
        break;
      }
    }
    if (already_requested) {
      continue;
    }
    requested_note_values[requested_count] = note_value;
    requested_midi_notes[requested_count] = midi_note;
    ++requested_count;
  }

  if (requested_count == 0) {
    noteOff();
    return;
  }

  const std::size_t clamped_count = requested_count;

  if (active_source_type_ == AudioSourceType::ExternalI2S) {
    const float frequency = noteValueToFrequency(requested_note_values[0]);
    if (!voice_active_[0]) {
      if (!source.noteOn(0, requested_note_values[0], frequency, waveform)) {
        stopAllImmediately();
        return;
      }
      envelopes_[0].noteOn();
    }
    voice_active_[0] = true;
    voice_held_[0] = true;
    active_midi_notes_[0] = requested_midi_notes[0];
    active_note_values_[0] = requested_note_values[0];
    active_frequencies_[0] = frequency;
    last_retrigger_ms_[0] = millis();
    voice_started_order_[0] = next_voice_order_++;
    active_waveform_ = waveform;
    for (std::size_t i = 1; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (voice_active_[i] && voice_held_[i]) {
        voice_held_[i] = false;
        envelopes_[i].noteOff();
      }
    }
    return;
  }

  std::array<std::size_t, SynthConfig::audio.polyphony_voices> held_voice_indices{};
  std::size_t held_voice_count = 0;
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i] && voice_held_[i]) {
      held_voice_indices[held_voice_count++] = i;
    }
  }

  // Keep hold-order stable so sliding touches update existing voices instead of retriggering envelopes.
  for (std::size_t i = 0; i < held_voice_count; ++i) {
    for (std::size_t j = i + 1; j < held_voice_count; ++j) {
      if (voice_started_order_[held_voice_indices[j]] < voice_started_order_[held_voice_indices[i]]) {
        const std::size_t tmp = held_voice_indices[i];
        held_voice_indices[i] = held_voice_indices[j];
        held_voice_indices[j] = tmp;
      }
    }
  }

  auto pick_voice_for_new_note = [&](int preferred_midi_note) -> std::size_t {
    for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (!voice_active_[i]) {
        return i;
      }
    }

    std::size_t matching_release = npos;
    std::uint32_t matching_release_order = UINT32_MAX;
    for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (voice_active_[i] && !voice_held_[i] && active_midi_notes_[i] == preferred_midi_note &&
          voice_started_order_[i] < matching_release_order) {
        matching_release_order = voice_started_order_[i];
        matching_release = i;
      }
    }
    if (matching_release != npos) {
      return matching_release;
    }

    std::size_t oldest_release = npos;
    std::uint32_t oldest_release_order = UINT32_MAX;
    for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (voice_active_[i] && !voice_held_[i] && voice_started_order_[i] < oldest_release_order) {
        oldest_release_order = voice_started_order_[i];
        oldest_release = i;
      }
    }
    if (oldest_release != npos) {
      return oldest_release;
    }

    std::size_t oldest_held = 0;
    std::uint32_t oldest_held_order = UINT32_MAX;
    for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (voice_started_order_[i] < oldest_held_order) {
        oldest_held_order = voice_started_order_[i];
        oldest_held = i;
      }
    }
    return oldest_held;
  };

  std::array<std::size_t, SynthConfig::audio.polyphony_voices> assigned_voice_indices{};
  assigned_voice_indices.fill(npos);
  std::array<bool, SynthConfig::audio.polyphony_voices> held_voice_used{};
  held_voice_used.fill(false);

  for (std::size_t j = 0; j < clamped_count; ++j) {
    for (std::size_t h = 0; h < held_voice_count; ++h) {
      if (held_voice_used[h]) {
        continue;
      }
      const std::size_t voice_index = held_voice_indices[h];
      if (active_midi_notes_[voice_index] == requested_midi_notes[j]) {
        assigned_voice_indices[j] = voice_index;
        held_voice_used[h] = true;
        break;
      }
    }
  }

  for (std::size_t j = 0; j < clamped_count; ++j) {
    if (assigned_voice_indices[j] != npos) {
      continue;
    }
    for (std::size_t h = 0; h < held_voice_count; ++h) {
      if (held_voice_used[h]) {
        continue;
      }
      assigned_voice_indices[j] = held_voice_indices[h];
      held_voice_used[h] = true;
      break;
    }
  }

  for (std::size_t h = 0; h < held_voice_count; ++h) {
    if (held_voice_used[h]) {
      continue;
    }
    const std::size_t voice_index = held_voice_indices[h];
    voice_held_[voice_index] = false;
    envelopes_[voice_index].noteOff();
  }

  for (std::size_t j = 0; j < clamped_count; ++j) {
    const std::size_t voice_index = assigned_voice_indices[j];
    if (voice_index == npos) {
      continue;
    }
    const float note_value = requested_note_values[j];
    const float frequency = noteValueToFrequency(note_value);
    const float note_delta = std::fabs(active_note_values_[voice_index] - note_value);
    const bool note_changed = note_delta >= kPitchRetuneThresholdSemitone;
    const bool waveform_changed = active_waveform_ != waveform;
    const std::uint32_t now = millis();
    const bool retune_interval_elapsed = (now - last_retrigger_ms_[voice_index]) >= kMinRetuneIntervalMs;

    // For glide / repeated taps: update pitch without envelope reset to avoid clicks.
    if ((note_changed && retune_interval_elapsed) || waveform_changed) {
      if (!source.noteOn(voice_index, note_value, frequency, waveform)) {
        voice_active_[voice_index] = false;
        voice_held_[voice_index] = false;
        active_midi_notes_[voice_index] = -1;
        active_note_values_[voice_index] = 0.0f;
        active_frequencies_[voice_index] = 0.0f;
        last_retrigger_ms_[voice_index] = 0;
        voice_started_order_[voice_index] = 0;
        envelopes_[voice_index].reset();
        continue;
      }
      last_retrigger_ms_[voice_index] = now;
    }

    voice_active_[voice_index] = true;
    voice_held_[voice_index] = true;
    active_midi_notes_[voice_index] = requested_midi_notes[j];
    active_note_values_[voice_index] = note_value;
    active_frequencies_[voice_index] = frequency;
  }

  for (std::size_t j = 0; j < clamped_count; ++j) {
    if (assigned_voice_indices[j] != npos) {
      continue;
    }
    const std::size_t voice_index = pick_voice_for_new_note(requested_midi_notes[j]);
    source.setVoiceLevel(voice_index, 0.0f, waveform);

    const float note_value = requested_note_values[j];
    const float frequency = noteValueToFrequency(note_value);
    envelopes_[voice_index].reset();
    envelopes_[voice_index].noteOn();
    if (!source.noteOn(voice_index, note_value, frequency, waveform)) {
      voice_active_[voice_index] = false;
      voice_held_[voice_index] = false;
      active_midi_notes_[voice_index] = -1;
      active_note_values_[voice_index] = 0.0f;
      active_frequencies_[voice_index] = 0.0f;
      last_retrigger_ms_[voice_index] = 0;
      voice_started_order_[voice_index] = 0;
      envelopes_[voice_index].reset();
      continue;
    }

    voice_active_[voice_index] = true;
    voice_held_[voice_index] = true;
    active_midi_notes_[voice_index] = requested_midi_notes[j];
    active_note_values_[voice_index] = note_value;
    active_frequencies_[voice_index] = frequency;
    last_retrigger_ms_[voice_index] = millis();
    voice_started_order_[voice_index] = next_voice_order_++;
  }

  active_waveform_ = waveform;
}

void AudioEngine::noteOff() {
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i] && voice_held_[i]) {
      voice_held_[i] = false;
      envelopes_[i].noteOff();
    }
  }
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

  stopAllImmediately();
  active_source_type_ = source_type;
}

void AudioEngine::setAttackNormalized(float normalized) {
  amp_envelope_.attack_seconds = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_attack_max_ms) / 1000.0f;
  applyEnvelopeSettings();
}

void AudioEngine::setDecayNormalized(float normalized) {
  amp_envelope_.decay_seconds = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_decay_max_ms) / 1000.0f;
  applyEnvelopeSettings();
}

void AudioEngine::setSustainNormalized(float normalized) {
  amp_envelope_.sustain_level = std::clamp(normalized, 0.0f, 1.0f);
  applyEnvelopeSettings();
}

void AudioEngine::setReleaseNormalized(float normalized) {
  amp_envelope_.release_seconds = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_release_max_ms) / 1000.0f;
  applyEnvelopeSettings();
}

bool AudioEngine::beginMicSampleRecording() {
  stopAllImmediately();
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
  stopAllImmediately();
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

float AudioEngine::attackNormalized() const {
  return millisecondsToNormalized(amp_envelope_.attack_seconds * 1000.0f, SynthConfig::audio.amp_attack_max_ms);
}

float AudioEngine::decayNormalized() const {
  return millisecondsToNormalized(amp_envelope_.decay_seconds * 1000.0f, SynthConfig::audio.amp_decay_max_ms);
}

float AudioEngine::sustainNormalized() const {
  return std::clamp(amp_envelope_.sustain_level, 0.0f, 1.0f);
}

float AudioEngine::releaseNormalized() const {
  return millisecondsToNormalized(amp_envelope_.release_seconds * 1000.0f, SynthConfig::audio.amp_release_max_ms);
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

float AudioEngine::normalizedToMilliseconds(float normalized, float max_ms) {
  return std::clamp(normalized, 0.0f, 1.0f) * max_ms;
}

float AudioEngine::millisecondsToNormalized(float value_ms, float max_ms) {
  if (max_ms <= 0.0f) {
    return 0.0f;
  }
  return std::clamp(value_ms / max_ms, 0.0f, 1.0f);
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

void AudioEngine::applyEnvelopeSettings() {
  for (auto& envelope : envelopes_) {
    envelope.setSettings(amp_envelope_);
  }
}

void AudioEngine::stopAllImmediately() {
  sourceFor(active_source_type_).noteOffAll();
  clearVoices();
}

void AudioEngine::clearVoices() {
  voice_active_.fill(false);
  voice_held_.fill(false);
  active_midi_notes_.fill(-1);
  active_note_values_.fill(0.0f);
  active_frequencies_.fill(0.0f);
  last_retrigger_ms_.fill(0);
  voice_started_order_.fill(0);
  next_voice_order_ = 1;
  for (auto& envelope : envelopes_) {
    envelope.reset();
  }
}
