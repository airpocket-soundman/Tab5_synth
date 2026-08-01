#pragma once

#include "AudioSource.h"
#include "EnvelopeGenerator.h"
#include "I2sTransport.h"
#include "StreamEffects.h"
#include "StreamTransport.h"
#include "SynthConfig.h"
#include "UdpTransport.h"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>

enum class StreamTransportKind : std::uint8_t {
  I2s,
  Udp,
};

// External audio source: pulls 16bit/mono frames from a pluggable transport
// (wired I2S or WiFi UDP), buffers them in a ring to decouple sender and
// speaker clocks, applies the stream effect chain, and plays the result.
class ExternalStreamSource : public AudioSource {
 public:
  bool begin() override;
  bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) override;
  void noteOff(std::size_t voice_index) override;
  void noteOffAll() override;
  void setVolume(float volume) override;
  void setVoiceLevel(std::size_t voice_index, float level, Waveform waveform) override;
  bool isAvailable() const override;
  AudioSourceType type() const override;

  StreamEffects& effects() { return effects_; }
  void setEnvelopeSettings(const EnvelopeSettings& settings);
  void setTransportKind(StreamTransportKind kind);
  StreamTransportKind transportKind() const { return transport_kind_; }
  bool isKindAvailable(StreamTransportKind kind) const;
  void debugPrint() const;

 private:
#if defined(ESP_PLATFORM)
  static constexpr std::size_t kReadFrames = SynthConfig::audio.external_i2s_buffer_frames;
  // Ring buffer decouples the sender's clock from the speaker clock.
  // Prefill absorbs scheduling/network jitter; capacity absorbs drift.
  static constexpr std::size_t kRingCapacity = 8192;    // 512ms @16kHz
  static constexpr std::size_t kPrefillFrames = 1024;   // 64ms
  static constexpr std::size_t kPlayChunkFrames = 512;  // 32ms
  static constexpr std::size_t kPlayBufferCount = 4;

  // Per-voice state for the polyphonic UDP mixer.
  static constexpr std::size_t kUdpMixVoices = 4;
  static constexpr std::size_t kVoiceRingCapacity = 4096;  // 256ms per voice
  static constexpr std::size_t kVoicePrefillFrames = 512;  // 32ms
  struct UdpVoice {
    std::array<std::int16_t, kVoiceRingCapacity> ring{};
    std::size_t head = 0;
    std::size_t count = 0;
    bool primed = false;
    bool last_gate = false;
    EnvelopeGenerator envelope{};
  };

  static void monitorTaskEntry(void* arg);
  bool startMonitor();
  void stopMonitor();
  void monitorTask();
  void monitorLoopI2s();
  void monitorLoopUdp();
  void ringPush(std::int16_t sample);
  void ringPopChunk(std::int16_t* dest, std::size_t count);
  void voicePush(UdpVoice& voice, std::int16_t sample);
  StreamTransport& activeTransport();

  TaskHandle_t monitor_task_ = nullptr;
  std::array<std::int16_t, kReadFrames> read_buffer_{};
  std::array<std::int16_t, kRingCapacity> ring_{};
  std::size_t ring_head_ = 0;
  std::size_t ring_count_ = 0;
  std::array<std::array<std::int16_t, kPlayChunkFrames>, kPlayBufferCount> play_buffers_{};
  std::size_t play_index_ = 0;
  bool primed_ = false;
  std::array<UdpVoice, kUdpMixVoices> udp_voices_{};
  EnvelopeSettings stream_envelope_settings_{};
  // Debug: peak |sample| seen since the last debugPrint (pre/post effects).
  mutable volatile std::int16_t rx_peak_ = 0;
  mutable volatile std::int16_t out_peak_ = 0;
#endif

  I2sTransport i2s_transport_{};
  UdpTransport udp_transport_{};
  EnvelopeGenerator stream_envelope_{};
  bool last_gate_ = false;
  StreamTransportKind transport_kind_ = StreamTransportKind::I2s;
  bool initialized_ = false;
  bool monitoring_ = false;
  float volume_ = SynthConfig::audio.default_volume;
  StreamEffects effects_{};
};
