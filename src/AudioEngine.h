#pragma once

#include "AudioSource.h"
#include "AudioSourceType.h"
#include "EnvelopeGenerator.h"
#include "ExternalI2SSource.h"
#include "OnboardMicSource.h"
#include "OscillatorSource.h"
#include "SynthConfig.h"
#include "Waveform.h"

#include <array>
#include <cstddef>
#include <cstdint>

class AudioEngine {
 public:
  void begin();
  void update();
  void noteOnVoices(const float* note_values, std::size_t count, Waveform waveform);
  void noteOff();
  void setVolume(float volume);
  void setSourceType(AudioSourceType source_type);
  void setAttackNormalized(float normalized);
  void setDecayNormalized(float normalized);
  void setSustainNormalized(float normalized);
  void setReleaseNormalized(float normalized);
  void setDelayEnabled(bool enabled);
  void setDelayParameters(float time_normalized, float feedback_normalized, float mix_normalized);
  void setChorusEnabled(bool enabled);
  void setChorusParameters(float rate_normalized, float depth_normalized, float mix_normalized);
  void setFilterEnabled(bool enabled);
  void setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized);
  void setDistortionEnabled(bool enabled);
  void setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized);
  void setBitcrusherEnabled(bool enabled);
  void setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized);
  bool beginMicSampleRecording();
  void updateMicSampleRecording();
  bool finishMicSampleRecording(bool commit_sample = true);
  bool recordMicSample();
  bool hasMicSample() const;
  bool isMicRecording() const;

  [[nodiscard]] bool isNotePlaying() const;
  [[nodiscard]] bool isCurrentSourceAvailable() const;
  [[nodiscard]] bool isSourceAvailable(AudioSourceType source_type) const;
  [[nodiscard]] int activeMidiNote() const;
  [[nodiscard]] float activeFrequency() const;
  [[nodiscard]] Waveform activeWaveform() const;
  [[nodiscard]] float volume() const;
  [[nodiscard]] float attackNormalized() const;
  [[nodiscard]] float decayNormalized() const;
  [[nodiscard]] float sustainNormalized() const;
  [[nodiscard]] float releaseNormalized() const;
  [[nodiscard]] AudioSourceType activeSourceType() const;
  [[nodiscard]] std::size_t activeVoiceCount() const;
  [[nodiscard]] bool delayEnabled() const;
  [[nodiscard]] float delayTimeNormalized() const;
  [[nodiscard]] float delayFeedbackNormalized() const;
  [[nodiscard]] float delayMixNormalized() const;
  [[nodiscard]] bool chorusEnabled() const;
  [[nodiscard]] float chorusRateNormalized() const;
  [[nodiscard]] float chorusDepthNormalized() const;
  [[nodiscard]] float chorusMixNormalized() const;
  [[nodiscard]] bool filterEnabled() const;
  [[nodiscard]] float filterCutoffNormalized() const;
  [[nodiscard]] float filterResonanceNormalized() const;
  [[nodiscard]] float filterMixNormalized() const;

 private:
  struct PendingDelayEvent {
    bool active = false;
    std::uint32_t fire_ms = 0;
    AudioSourceType source_type = AudioSourceType::Oscillator;
    float note_value = 0.0f;
    float frequency = 0.0f;
    Waveform waveform = Waveform::Sine;
    float gain = 0.0f;
    std::uint8_t repeats_left = 0;
  };

  struct PendingVoiceOff {
    bool active = false;
    std::uint32_t fire_ms = 0;
    AudioSourceType source_type = AudioSourceType::Oscillator;
    std::size_t voice_index = 0;
  };

  static float noteValueToFrequency(float note_value);
  static float normalizedToMilliseconds(float normalized, float max_ms);
  static float millisecondsToNormalized(float value_ms, float max_ms);
  AudioSource& sourceFor(AudioSourceType source_type);
  const AudioSource& sourceFor(AudioSourceType source_type) const;
  void applyVoiceLevel(std::size_t voice_index, float envelope_value, Waveform waveform);
  void resetVoice(std::size_t voice_index);
  std::size_t pickVoiceForEcho() const;
  void enqueueDelayEvent(float note_value, float frequency, Waveform waveform, std::uint32_t now_ms);
  void processDelayEvents(std::uint32_t now_ms);
  void triggerDelayEvent(const PendingDelayEvent& event, std::uint32_t now_ms);
  void processChorus(std::uint32_t now_ms, float delta_seconds);
  void refreshOscillatorVoicesTimbre();
  void scheduleVoiceOff(std::size_t voice_index, std::uint32_t fire_ms);
  void clearPendingVoiceOff(std::size_t voice_index);
  void applyEnvelopeSettings();
  void stopAllImmediately();
  void clearVoices();

  std::array<bool, SynthConfig::audio.polyphony_voices> voice_active_{};
  std::array<bool, SynthConfig::audio.polyphony_voices> voice_held_{};
  std::array<float, SynthConfig::audio.polyphony_voices> voice_gain_{};
  std::array<bool, SynthConfig::audio.polyphony_voices> voice_is_delay_{};
  std::array<int, SynthConfig::audio.polyphony_voices> active_midi_notes_{};
  std::array<float, SynthConfig::audio.polyphony_voices> active_note_values_{};
  std::array<float, SynthConfig::audio.polyphony_voices> active_frequencies_{};
  std::array<std::uint32_t, SynthConfig::audio.polyphony_voices> last_retrigger_ms_{};
  std::array<std::uint32_t, SynthConfig::audio.polyphony_voices> voice_started_order_{};
  std::uint32_t next_voice_order_ = 1;
  std::array<EnvelopeGenerator, SynthConfig::audio.polyphony_voices> envelopes_{};
  EnvelopeSettings amp_envelope_{};
  Waveform active_waveform_ = Waveform::Sine;
  float volume_ = SynthConfig::audio.default_volume;
  AudioSourceType active_source_type_ = AudioSourceType::Oscillator;
  std::uint32_t last_envelope_update_ms_ = 0;
  OscillatorSource oscillator_source_{};
  OnboardMicSource onboard_mic_source_{};
  ExternalI2SSource external_i2s_source_{};
  bool delay_enabled_ = true;
  float delay_time_normalized_ = 0.35f;
  float delay_feedback_normalized_ = 0.40f;
  float delay_mix_normalized_ = 0.30f;
  bool chorus_enabled_ = true;
  float chorus_rate_normalized_ = 0.30f;
  float chorus_depth_normalized_ = 0.40f;
  float chorus_mix_normalized_ = 0.30f;
  bool filter_enabled_ = true;
  float filter_cutoff_normalized_ = 1.00f;
  float filter_resonance_normalized_ = 0.25f;
  float filter_mix_normalized_ = 0.45f;
  bool distortion_enabled_ = false;
  float distortion_drive_normalized_ = 0.40f;
  float distortion_tone_normalized_ = 0.55f;
  float distortion_mix_normalized_ = 0.00f;
  bool bitcrusher_enabled_ = false;
  float bitcrusher_bits_normalized_ = 1.00f;
  float bitcrusher_rate_normalized_ = 1.00f;
  float bitcrusher_mix_normalized_ = 0.00f;
  std::array<float, SynthConfig::audio.polyphony_voices> chorus_phase_{};
  std::array<float, SynthConfig::audio.polyphony_voices> chorus_last_note_value_{};
  std::array<std::uint32_t, SynthConfig::audio.polyphony_voices> chorus_last_retune_ms_{};
  std::array<PendingDelayEvent, SynthConfig::audio.polyphony_voices * 4> pending_delay_events_{};
  std::array<PendingVoiceOff, SynthConfig::audio.polyphony_voices * 2> pending_voice_off_{};
};
