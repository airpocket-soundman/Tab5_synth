#include "AudioEngine.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPitchRetuneThresholdSemitone = 0.25f;

bool isExternalStream(AudioSourceType type) {
  return type == AudioSourceType::ExternalI2S || type == AudioSourceType::ExternalUdp;
}

constexpr std::uint32_t kMinRetuneIntervalMs = 24;
constexpr float kDelayMinMs = 40.0f;
constexpr float kDelayMaxMs = 700.0f;
constexpr std::uint32_t kEchoGateMinMs = 18;
// Echo chain cutoff: keep re-triggering until the echo gain rounds below
// one step of the 8-bit channel volume (1/255 ~= 0.004) — quieter echoes
// physically cannot be reproduced.
constexpr float kMinDelayGain = 0.004f;
constexpr float kTwoPi = 6.28318530718f;
constexpr std::uint32_t kChorusRetuneMinMs = 8;
constexpr float kChorusRetuneThresholdSemitone = 0.004f;
}

void AudioEngine::begin() {
  mutex_ = xSemaphoreCreateRecursiveMutex();
  auto speaker_config = M5.Speaker.config();
  M5.Speaker.end();
  // Channel gain is sampled once per DMA block by M5Unified. Keep blocks short
  // enough that ADSR changes do not become audible steps at note onset.
  speaker_config.dma_buf_len = 256;
  speaker_config.dma_buf_count = 16;
  speaker_config.task_priority = 12;
  speaker_config.task_pinned_core = 0;
  M5.Speaker.config(speaker_config);
  M5.Speaker.begin();
  M5.Speaker.setVolume(SynthConfig::audio.speaker_volume);
  amp_envelope_.attack_seconds = SynthConfig::audio.amp_attack_default_ms / 1000.0f;
  amp_envelope_.decay_seconds = SynthConfig::audio.amp_decay_default_ms / 1000.0f;
  amp_envelope_.sustain_level = SynthConfig::audio.amp_sustain_default;
  amp_envelope_.release_seconds = SynthConfig::audio.amp_release_default_ms / 1000.0f;
  clearVoices();
  applyEnvelopeSettings();
  oscillator_source_.begin();
  onboard_mic_source_.begin();
  external_stream_source_.begin();
  oscillator_source_.setVolume(volume_);
  oscillator_source_.setFilterEnabled(filter_enabled_);
  oscillator_source_.setFilterParameters(filter_cutoff_normalized_, filter_resonance_normalized_, filter_mix_normalized_);
  oscillator_source_.setDistortionEnabled(distortion_enabled_);
  oscillator_source_.setDistortionParameters(distortion_drive_normalized_, distortion_tone_normalized_,
                                             distortion_mix_normalized_);
  oscillator_source_.setBitcrusherEnabled(bitcrusher_enabled_);
  oscillator_source_.setBitcrusherParameters(bitcrusher_bits_normalized_, bitcrusher_rate_normalized_,
                                             bitcrusher_mix_normalized_);
  onboard_mic_source_.setVolume(volume_);
  external_stream_source_.setVolume(volume_);
  // Mirror current effect settings into the external I2S stream chain.
  auto& stream_fx = external_stream_source_.effects();
  stream_fx.setDelayEnabled(delay_enabled_);
  stream_fx.setDelayParameters(delay_time_normalized_, delay_feedback_normalized_, delay_mix_normalized_);
  stream_fx.setChorusEnabled(chorus_enabled_);
  stream_fx.setChorusParameters(chorus_rate_normalized_, chorus_depth_normalized_, chorus_mix_normalized_);
  stream_fx.setFilterEnabled(filter_enabled_);
  stream_fx.setFilterParameters(filter_cutoff_normalized_, filter_resonance_normalized_, filter_mix_normalized_);
  stream_fx.setDistortionEnabled(distortion_enabled_);
  stream_fx.setDistortionParameters(distortion_drive_normalized_, distortion_tone_normalized_,
                                    distortion_mix_normalized_);
  stream_fx.setBitcrusherEnabled(bitcrusher_enabled_);
  stream_fx.setBitcrusherParameters(bitcrusher_bits_normalized_, bitcrusher_rate_normalized_,
                                    bitcrusher_mix_normalized_);
  last_envelope_update_ms_ = millis();
  xTaskCreatePinnedToCore(audioTaskEntry, "synth_audio", 6144, this, 10, &audio_task_, 0);
}

void AudioEngine::update() {
  // Audio processing runs independently on core 0.
}

void AudioEngine::audioTaskEntry(void* context) {
  auto* engine = static_cast<AudioEngine*>(context);
  TickType_t wake_time = xTaskGetTickCount();
  for (;;) {
    engine->processAudio();
    vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(1));
  }
}

void AudioEngine::processAudio() {
  ScopedLock lock(mutex_);
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
      resetVoice(i);
      continue;
    }

    applyVoiceLevel(i, env, active_waveform_);
  }

  processChorus(now, delta_seconds);
  processDelayEvents(now);
}

void AudioEngine::noteOnVoices(const float* note_values, std::size_t count, Waveform waveform) {
  ScopedLock lock(mutex_);
  AudioSource& source = sourceFor(active_source_type_);
  if (isExternalStream(active_source_type_)) {
    if (!source.isAvailable()) {
      return;
    }
    // Always-on monitor mode for external streams (I2S/UDP):
    // keep receiver active regardless of keyboard touches.
    source.noteOn(0, 0.0f, 0.0f, waveform);
    active_waveform_ = waveform;
    return;
  }

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

  if (isExternalStream(active_source_type_)) {
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
    voice_gain_[0] = 1.0f;
    voice_is_delay_[0] = false;
    active_midi_notes_[0] = requested_midi_notes[0];
    active_note_values_[0] = requested_note_values[0];
    active_frequencies_[0] = frequency;
    chorus_last_note_value_[0] = requested_note_values[0];
    chorus_last_retune_ms_[0] = millis();
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

  // Reuse held voices only for the same MIDI note.
  // For different notes, release the previous held voice and allocate a new voice
  // so note tails can overlap naturally.

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
        resetVoice(voice_index);
        envelopes_[voice_index].reset();
        continue;
      }
      last_retrigger_ms_[voice_index] = now;
    }

    voice_active_[voice_index] = true;
    voice_held_[voice_index] = true;
    voice_gain_[voice_index] = 1.0f;
    voice_is_delay_[voice_index] = false;
    active_midi_notes_[voice_index] = requested_midi_notes[j];
    active_note_values_[voice_index] = note_value;
    active_frequencies_[voice_index] = frequency;
    chorus_last_note_value_[voice_index] = note_value;
    chorus_last_retune_ms_[voice_index] = millis();
  }

  bool starts_new_voice = false;
  for (std::size_t j = 0; j < clamped_count; ++j) {
    starts_new_voice = starts_new_voice || assigned_voice_indices[j] == npos;
  }
  if (starts_new_voice) {
    for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
      if (!voice_active_[i]) {
        source.setVoiceLevel(i, 0.0f, waveform);
      }
    }
    // Let the mixer consume one silent DMA block before it sees a new wave.
    delay(6);
  }

  for (std::size_t j = 0; j < clamped_count; ++j) {
    if (assigned_voice_indices[j] != npos) {
      continue;
    }
    const std::size_t voice_index = pick_voice_for_new_note(requested_midi_notes[j]);
    source.setVoiceLevel(voice_index, 0.0f, waveform);
    clearPendingVoiceOff(voice_index);

    const float note_value = requested_note_values[j];
    const float frequency = noteValueToFrequency(note_value);
    envelopes_[voice_index].reset();
    envelopes_[voice_index].noteOn();
    if (!source.noteOn(voice_index, note_value, frequency, waveform)) {
      resetVoice(voice_index);
      envelopes_[voice_index].reset();
      continue;
    }

    voice_active_[voice_index] = true;
    voice_held_[voice_index] = true;
    voice_gain_[voice_index] = 1.0f;
    voice_is_delay_[voice_index] = false;
    active_midi_notes_[voice_index] = requested_midi_notes[j];
    active_note_values_[voice_index] = note_value;
    active_frequencies_[voice_index] = frequency;
    chorus_last_note_value_[voice_index] = note_value;
    chorus_last_retune_ms_[voice_index] = millis();
    last_retrigger_ms_[voice_index] = millis();
    voice_started_order_[voice_index] = next_voice_order_++;
    enqueueDelayEvent(note_value, frequency, waveform, millis());
  }

  active_waveform_ = waveform;
}

void AudioEngine::noteOff() {
  ScopedLock lock(mutex_);
  if (isExternalStream(active_source_type_)) {
    // Keep monitoring while an external stream source is selected.
    return;
  }
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i] && voice_held_[i]) {
      voice_held_[i] = false;
      envelopes_[i].noteOff();
    }
  }
}

void AudioEngine::setVolume(float volume) {
  ScopedLock lock(mutex_);
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  oscillator_source_.setVolume(volume_);
  onboard_mic_source_.setVolume(volume_);
  external_stream_source_.setVolume(volume_);
}

void AudioEngine::setSourceType(AudioSourceType source_type) {
  ScopedLock lock(mutex_);
  if (active_source_type_ == source_type) {
    if (isExternalStream(source_type)) {
      sourceFor(active_source_type_).noteOn(0, 0.0f, 0.0f, active_waveform_);
    }
    return;
  }

  stopAllImmediately();
  active_source_type_ = source_type;
  if (isExternalStream(active_source_type_)) {
    external_stream_source_.setTransportKind(active_source_type_ == AudioSourceType::ExternalUdp
                                                 ? StreamTransportKind::Udp
                                                 : StreamTransportKind::I2s);
    sourceFor(active_source_type_).noteOn(0, 0.0f, 0.0f, active_waveform_);
  }
}

void AudioEngine::setInstrumentTimbre(InstrumentTimbre timbre) {
  ScopedLock lock(mutex_);
  if (instrument_timbre_ == timbre) {
    return;
  }
  instrument_timbre_ = timbre;
  oscillator_source_.setInstrumentTimbre(timbre);
  refreshOscillatorVoicesTimbre();
}

void AudioEngine::setAttackNormalized(float normalized) {
  ScopedLock lock(mutex_);
  const float next = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_attack_max_ms) / 1000.0f;
  if (std::fabs(next - amp_envelope_.attack_seconds) < 0.0005f) {
    return;
  }
  amp_envelope_.attack_seconds = next;
  applyEnvelopeSettings();
}

void AudioEngine::setDecayNormalized(float normalized) {
  ScopedLock lock(mutex_);
  const float next = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_decay_max_ms) / 1000.0f;
  if (std::fabs(next - amp_envelope_.decay_seconds) < 0.0005f) {
    return;
  }
  amp_envelope_.decay_seconds = next;
  applyEnvelopeSettings();
}

void AudioEngine::setSustainNormalized(float normalized) {
  ScopedLock lock(mutex_);
  const float next = std::clamp(normalized, 0.0f, 1.0f);
  if (std::fabs(next - amp_envelope_.sustain_level) < 0.0005f) {
    return;
  }
  amp_envelope_.sustain_level = next;
  applyEnvelopeSettings();
}

void AudioEngine::setReleaseNormalized(float normalized) {
  ScopedLock lock(mutex_);
  const float next = normalizedToMilliseconds(normalized, SynthConfig::audio.amp_release_max_ms) / 1000.0f;
  if (std::fabs(next - amp_envelope_.release_seconds) < 0.0005f) {
    return;
  }
  amp_envelope_.release_seconds = next;
  applyEnvelopeSettings();
}

void AudioEngine::setDelayEnabled(bool enabled) {
  ScopedLock lock(mutex_);
  delay_enabled_ = enabled;
  external_stream_source_.effects().setDelayEnabled(enabled);
}

void AudioEngine::setDelayParameters(float time_normalized, float feedback_normalized, float mix_normalized) {
  ScopedLock lock(mutex_);
  delay_time_normalized_ = std::clamp(time_normalized, 0.0f, 1.0f);
  delay_feedback_normalized_ = std::clamp(feedback_normalized, 0.0f, 1.0f);
  delay_mix_normalized_ = std::clamp(mix_normalized, 0.0f, 1.0f);
  external_stream_source_.effects().setDelayParameters(delay_time_normalized_, delay_feedback_normalized_,
                                                    delay_mix_normalized_);
}

void AudioEngine::setChorusEnabled(bool enabled) {
  ScopedLock lock(mutex_);
  if (chorus_enabled_ == enabled) {
    return;
  }
  chorus_enabled_ = enabled;
  external_stream_source_.effects().setChorusEnabled(enabled);
  if (chorus_enabled_) {
    return;
  }

  if (active_source_type_ != AudioSourceType::Oscillator) {
    return;
  }

  AudioSource& source = sourceFor(active_source_type_);
  const std::uint32_t now_ms = millis();
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (!voice_active_[i] || !voice_held_[i] || voice_is_delay_[i]) {
      continue;
    }
    const float base_note = active_note_values_[i];
    const float base_freq = noteValueToFrequency(base_note);
    if (!source.noteOn(i, base_note, base_freq, active_waveform_)) {
      continue;
    }
    chorus_last_note_value_[i] = base_note;
    chorus_last_retune_ms_[i] = now_ms;
  }
}

void AudioEngine::setChorusParameters(float rate_normalized, float depth_normalized, float mix_normalized) {
  ScopedLock lock(mutex_);
  chorus_rate_normalized_ = std::clamp(rate_normalized, 0.0f, 1.0f);
  chorus_depth_normalized_ = std::clamp(depth_normalized, 0.0f, 1.0f);
  chorus_mix_normalized_ = std::clamp(mix_normalized, 0.0f, 1.0f);
  external_stream_source_.effects().setChorusParameters(chorus_rate_normalized_, chorus_depth_normalized_,
                                                     chorus_mix_normalized_);
}

void AudioEngine::setFilterEnabled(bool enabled) {
  ScopedLock lock(mutex_);
  if (filter_enabled_ == enabled) {
    return;
  }
  filter_enabled_ = enabled;
  oscillator_source_.setFilterEnabled(filter_enabled_);
  external_stream_source_.effects().setFilterEnabled(filter_enabled_);
  refreshOscillatorVoicesTimbre();
}

void AudioEngine::setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized) {
  ScopedLock lock(mutex_);
  const float next_cutoff = std::clamp(cutoff_normalized, 0.0f, 1.0f);
  const float next_resonance = std::clamp(resonance_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_cutoff - filter_cutoff_normalized_) < 0.0005f &&
      std::fabs(next_resonance - filter_resonance_normalized_) < 0.0005f &&
      std::fabs(next_mix - filter_mix_normalized_) < 0.0005f) {
    return;
  }
  filter_cutoff_normalized_ = next_cutoff;
  filter_resonance_normalized_ = next_resonance;
  filter_mix_normalized_ = next_mix;
  oscillator_source_.setFilterParameters(filter_cutoff_normalized_, filter_resonance_normalized_, filter_mix_normalized_);
  external_stream_source_.effects().setFilterParameters(filter_cutoff_normalized_, filter_resonance_normalized_,
                                                     filter_mix_normalized_);
  if (filter_enabled_ && filter_mix_normalized_ > 0.0f) {
    refreshOscillatorVoicesTimbre();
  }
}

void AudioEngine::setDistortionEnabled(bool enabled) {
  ScopedLock lock(mutex_);
  if (distortion_enabled_ == enabled) {
    return;
  }
  distortion_enabled_ = enabled;
  oscillator_source_.setDistortionEnabled(distortion_enabled_);
  external_stream_source_.effects().setDistortionEnabled(distortion_enabled_);
  refreshOscillatorVoicesTimbre();
}

void AudioEngine::setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized) {
  ScopedLock lock(mutex_);
  const float next_drive = std::clamp(drive_normalized, 0.0f, 1.0f);
  const float next_tone = std::clamp(tone_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_drive - distortion_drive_normalized_) < 0.0005f &&
      std::fabs(next_tone - distortion_tone_normalized_) < 0.0005f &&
      std::fabs(next_mix - distortion_mix_normalized_) < 0.0005f) {
    return;
  }
  distortion_drive_normalized_ = next_drive;
  distortion_tone_normalized_ = next_tone;
  distortion_mix_normalized_ = next_mix;
  oscillator_source_.setDistortionParameters(distortion_drive_normalized_, distortion_tone_normalized_,
                                             distortion_mix_normalized_);
  external_stream_source_.effects().setDistortionParameters(distortion_drive_normalized_, distortion_tone_normalized_,
                                                         distortion_mix_normalized_);
  if (distortion_enabled_ && distortion_mix_normalized_ > 0.0f) {
    refreshOscillatorVoicesTimbre();
  }
}

void AudioEngine::setBitcrusherEnabled(bool enabled) {
  ScopedLock lock(mutex_);
  if (bitcrusher_enabled_ == enabled) {
    return;
  }
  bitcrusher_enabled_ = enabled;
  oscillator_source_.setBitcrusherEnabled(bitcrusher_enabled_);
  external_stream_source_.effects().setBitcrusherEnabled(bitcrusher_enabled_);
  refreshOscillatorVoicesTimbre();
}

void AudioEngine::setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized) {
  ScopedLock lock(mutex_);
  const float next_bits = std::clamp(bits_normalized, 0.0f, 1.0f);
  const float next_rate = std::clamp(rate_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_bits - bitcrusher_bits_normalized_) < 0.0005f &&
      std::fabs(next_rate - bitcrusher_rate_normalized_) < 0.0005f &&
      std::fabs(next_mix - bitcrusher_mix_normalized_) < 0.0005f) {
    return;
  }
  bitcrusher_bits_normalized_ = next_bits;
  bitcrusher_rate_normalized_ = next_rate;
  bitcrusher_mix_normalized_ = next_mix;
  oscillator_source_.setBitcrusherParameters(bitcrusher_bits_normalized_, bitcrusher_rate_normalized_,
                                             bitcrusher_mix_normalized_);
  external_stream_source_.effects().setBitcrusherParameters(bitcrusher_bits_normalized_, bitcrusher_rate_normalized_,
                                                         bitcrusher_mix_normalized_);
  if (bitcrusher_enabled_ && bitcrusher_mix_normalized_ > 0.0f) {
    refreshOscillatorVoicesTimbre();
  }
}

bool AudioEngine::beginMicSampleRecording() {
  ScopedLock lock(mutex_);
  stopAllImmediately();
  const bool started = onboard_mic_source_.beginRecording();
  if (started) {
    active_source_type_ = AudioSourceType::OnboardMic;
  }
  return started;
}

void AudioEngine::updateMicSampleRecording() {
  ScopedLock lock(mutex_);
  onboard_mic_source_.updateRecording();
}

bool AudioEngine::finishMicSampleRecording(bool commit_sample) {
  ScopedLock lock(mutex_);
  const bool recorded = onboard_mic_source_.finishRecording(commit_sample);
  if (recorded) {
    active_source_type_ = AudioSourceType::OnboardMic;
  }
  return recorded;
}

bool AudioEngine::recordMicSample() {
  ScopedLock lock(mutex_);
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

bool AudioEngine::delayEnabled() const {
  return delay_enabled_;
}

float AudioEngine::delayTimeNormalized() const {
  return delay_time_normalized_;
}

float AudioEngine::delayFeedbackNormalized() const {
  return delay_feedback_normalized_;
}

float AudioEngine::delayMixNormalized() const {
  return delay_mix_normalized_;
}

bool AudioEngine::chorusEnabled() const {
  return chorus_enabled_;
}

float AudioEngine::chorusRateNormalized() const {
  return chorus_rate_normalized_;
}

float AudioEngine::chorusDepthNormalized() const {
  return chorus_depth_normalized_;
}

float AudioEngine::chorusMixNormalized() const {
  return chorus_mix_normalized_;
}

bool AudioEngine::filterEnabled() const {
  return filter_enabled_;
}

float AudioEngine::filterCutoffNormalized() const {
  return filter_cutoff_normalized_;
}

float AudioEngine::filterResonanceNormalized() const {
  return filter_resonance_normalized_;
}

float AudioEngine::filterMixNormalized() const {
  return filter_mix_normalized_;
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
    case AudioSourceType::ExternalUdp:
      return external_stream_source_;
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
    case AudioSourceType::ExternalUdp:
      return external_stream_source_;
    default:
      return oscillator_source_;
  }
}

void AudioEngine::applyEnvelopeSettings() {
  for (auto& envelope : envelopes_) {
    envelope.setSettings(amp_envelope_);
  }
  external_stream_source_.setEnvelopeSettings(amp_envelope_);
}

void AudioEngine::applyVoiceLevel(std::size_t voice_index, float envelope_value, Waveform waveform) {
  const float gain = std::clamp(voice_gain_[voice_index], 0.0f, 1.0f);
  sourceFor(active_source_type_).setVoiceLevel(voice_index, volume_ * envelope_value * gain, waveform);
}

void AudioEngine::resetVoice(std::size_t voice_index) {
  clearPendingVoiceOff(voice_index);
  voice_active_[voice_index] = false;
  voice_held_[voice_index] = false;
  voice_gain_[voice_index] = 1.0f;
  voice_is_delay_[voice_index] = false;
  active_midi_notes_[voice_index] = -1;
  active_note_values_[voice_index] = 0.0f;
  active_frequencies_[voice_index] = 0.0f;
  last_retrigger_ms_[voice_index] = 0;
  voice_started_order_[voice_index] = 0;
  chorus_phase_[voice_index] = 0.0f;
  chorus_last_note_value_[voice_index] = 0.0f;
  chorus_last_retune_ms_[voice_index] = 0;
}

std::size_t AudioEngine::pickVoiceForEcho() const {
  constexpr std::size_t npos = static_cast<std::size_t>(-1);
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (!voice_active_[i]) {
      return i;
    }
  }
  std::size_t oldest_release = npos;
  std::uint32_t oldest_release_order = UINT32_MAX;
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (voice_active_[i] && !voice_held_[i] && voice_started_order_[i] < oldest_release_order) {
      oldest_release_order = voice_started_order_[i];
      oldest_release = i;
    }
  }
  return oldest_release;
}

void AudioEngine::enqueueDelayEvent(float note_value, float frequency, Waveform waveform, std::uint32_t now_ms) {
  if (!delay_enabled_ || delay_mix_normalized_ <= 0.0f) {
    return;
  }
  const float delay_ms_f = kDelayMinMs + (kDelayMaxMs - kDelayMinMs) * delay_time_normalized_;
  const auto delay_ms = static_cast<std::uint32_t>(std::lround(delay_ms_f));
  // Generous repeat budget: the chain normally dies via the kMinDelayGain
  // cutoff as the per-echo gain decays; this cap only bounds the fb=max case
  // (a fixed 16 used to hard-stop FBK=100 tails at ~4s mid-volume).
  const std::uint8_t repeats = static_cast<std::uint8_t>(1 + static_cast<int>(std::lround(delay_feedback_normalized_ * 250.0f)));
  const float gain = std::clamp(delay_mix_normalized_ * 0.85f, 0.0f, 1.0f);
  if (gain < kMinDelayGain || repeats == 0) {
    return;
  }
  for (auto& event : pending_delay_events_) {
    if (event.active) {
      continue;
    }
    event.active = true;
    event.fire_ms = now_ms + delay_ms;
    event.source_type = active_source_type_;
    event.note_value = note_value;
    event.frequency = frequency;
    event.waveform = waveform;
    event.gain = gain;
    event.repeats_left = repeats;
    return;
  }
}

void AudioEngine::processDelayEvents(std::uint32_t now_ms) {
  for (auto& entry : pending_voice_off_) {
    if (!entry.active) {
      continue;
    }
    if (static_cast<std::int32_t>(now_ms - entry.fire_ms) < 0) {
      continue;
    }
    entry.active = false;
    if (entry.source_type != active_source_type_ || entry.voice_index >= SynthConfig::audio.polyphony_voices ||
        !voice_active_[entry.voice_index]) {
      continue;
    }
    voice_held_[entry.voice_index] = false;
    envelopes_[entry.voice_index].noteOff();
  }

  for (auto& event : pending_delay_events_) {
    if (!event.active) {
      continue;
    }
    if (static_cast<std::int32_t>(now_ms - event.fire_ms) < 0) {
      continue;
    }
    const PendingDelayEvent fired = event;
    event.active = false;
    if (!delay_enabled_ || fired.source_type != active_source_type_) {
      continue;
    }
    triggerDelayEvent(fired, now_ms);
  }
}

void AudioEngine::triggerDelayEvent(const PendingDelayEvent& event, std::uint32_t now_ms) {
  AudioSource& source = sourceFor(event.source_type);
  if (!source.isAvailable()) {
    return;
  }

  const std::size_t voice_index = pickVoiceForEcho();
  if (voice_index >= SynthConfig::audio.polyphony_voices) {
    return;
  }

  if (voice_active_[voice_index]) {
    source.noteOff(voice_index);
    resetVoice(voice_index);
  }

  envelopes_[voice_index].reset();
  envelopes_[voice_index].noteOn();
  if (!source.noteOn(voice_index, event.note_value, event.frequency, event.waveform)) {
    resetVoice(voice_index);
    return;
  }

  const int midi_note = static_cast<int>(std::lround(event.note_value));
  voice_active_[voice_index] = true;
  voice_held_[voice_index] = true;
  voice_gain_[voice_index] = std::clamp(event.gain, 0.0f, 1.0f);
  voice_is_delay_[voice_index] = true;
  active_midi_notes_[voice_index] = midi_note;
  active_note_values_[voice_index] = event.note_value;
  active_frequencies_[voice_index] = event.frequency;
  chorus_last_note_value_[voice_index] = event.note_value;
  chorus_last_retune_ms_[voice_index] = now_ms;
  last_retrigger_ms_[voice_index] = now_ms;
  voice_started_order_[voice_index] = next_voice_order_++;

  const float delay_ms_f = kDelayMinMs + (kDelayMaxMs - kDelayMinMs) * delay_time_normalized_;
  const std::uint32_t delay_ms = static_cast<std::uint32_t>(std::lround(delay_ms_f));
  const std::uint32_t gate_ms = std::max<std::uint32_t>(kEchoGateMinMs, delay_ms / 2);
  scheduleVoiceOff(voice_index, now_ms + gate_ms);

  if (event.repeats_left > 1) {
    // Cap the per-echo gain below 1.0 so even FBK=100 decays (~0.35dB/echo,
    // matching the stream chain's feedback ceiling) instead of repeating at
    // constant volume until the repeat budget runs out.
    const float next_gain = std::clamp(event.gain * std::min(delay_feedback_normalized_, 0.96f), 0.0f, 1.0f);
    if (next_gain >= kMinDelayGain) {
      PendingDelayEvent chained = event;
      chained.fire_ms = now_ms + delay_ms;
      chained.gain = next_gain;
      chained.repeats_left = static_cast<std::uint8_t>(event.repeats_left - 1);
      for (auto& slot : pending_delay_events_) {
        if (!slot.active) {
          slot = chained;
          slot.active = true;
          break;
        }
      }
    }
  }
}

void AudioEngine::processChorus(std::uint32_t now_ms, float delta_seconds) {
  // Reissuing tone() for pitch modulation restarts the M5 speaker channel and
  // produces periodic clicks. Keep chorus bypassed until it has a buffer-based DSP path.
  (void)now_ms;
  (void)delta_seconds;
}

void AudioEngine::refreshOscillatorVoicesTimbre() {
  if (active_source_type_ != AudioSourceType::Oscillator) {
    return;
  }
  AudioSource& source = sourceFor(active_source_type_);
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    if (!voice_active_[i]) {
      continue;
    }
    source.noteOn(i, active_note_values_[i], active_frequencies_[i], active_waveform_);
  }
}

void AudioEngine::scheduleVoiceOff(std::size_t voice_index, std::uint32_t fire_ms) {
  clearPendingVoiceOff(voice_index);
  for (auto& slot : pending_voice_off_) {
    if (slot.active) {
      continue;
    }
    slot.active = true;
    slot.fire_ms = fire_ms;
    slot.source_type = active_source_type_;
    slot.voice_index = voice_index;
    return;
  }
}

void AudioEngine::clearPendingVoiceOff(std::size_t voice_index) {
  for (auto& slot : pending_voice_off_) {
    if (slot.active && slot.voice_index == voice_index) {
      slot.active = false;
    }
  }
}

void AudioEngine::stopAllImmediately() {
  sourceFor(active_source_type_).noteOffAll();
  clearVoices();
}

void AudioEngine::clearVoices() {
  voice_active_.fill(false);
  voice_held_.fill(false);
  voice_gain_.fill(1.0f);
  voice_is_delay_.fill(false);
  active_midi_notes_.fill(-1);
  active_note_values_.fill(0.0f);
  active_frequencies_.fill(0.0f);
  last_retrigger_ms_.fill(0);
  voice_started_order_.fill(0);
  chorus_phase_.fill(0.0f);
  chorus_last_note_value_.fill(0.0f);
  chorus_last_retune_ms_.fill(0);
  for (auto& event : pending_delay_events_) {
    event.active = false;
  }
  for (auto& event : pending_voice_off_) {
    event.active = false;
  }
  next_voice_order_ = 1;
  for (auto& envelope : envelopes_) {
    envelope.reset();
  }
}
