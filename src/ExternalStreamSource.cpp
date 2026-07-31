#include "ExternalStreamSource.h"

#include "AudioSourceType.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

bool ExternalStreamSource::begin() {
#if defined(ESP_PLATFORM)
  effects_.begin(static_cast<float>(SynthConfig::audio.external_i2s_sample_rate));
  // I2S is cheap to bring up eagerly; WiFi (UDP) initializes lazily when the
  // UDP transport is first started.
  initialized_ = i2s_transport_.begin();
  setVolume(volume_);
  return initialized_;
#else
  initialized_ = false;
  return false;
#endif
}

bool ExternalStreamSource::noteOn(std::size_t /*voice_index*/, float /*note_value*/, float /*frequency*/,
                                  Waveform /*waveform*/) {
#if defined(ESP_PLATFORM)
  return startMonitor();
#else
  return false;
#endif
}

void ExternalStreamSource::noteOff(std::size_t /*voice_index*/) {
#if defined(ESP_PLATFORM)
  stopMonitor();
#endif
}

void ExternalStreamSource::noteOffAll() {
#if defined(ESP_PLATFORM)
  stopMonitor();
#endif
}

void ExternalStreamSource::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
#if defined(ESP_PLATFORM)
  // Apply immediately so the UI volume slider works while streaming.
  if (monitoring_) {
    setVoiceLevel(0, volume_, Waveform::Sine);
  }
#endif
}

void ExternalStreamSource::setVoiceLevel(std::size_t voice_index, float level, Waveform /*waveform*/) {
  const float scaled = std::clamp(level, 0.0f, 1.0f);
  M5.Speaker.setChannelVolume(SynthConfig::audio.audio_channel + static_cast<int>(voice_index),
                              static_cast<std::uint8_t>(std::lround(scaled * 255.0f)));
}

bool ExternalStreamSource::isAvailable() const {
  return isKindAvailable(transport_kind_);
}

AudioSourceType ExternalStreamSource::type() const {
  return transport_kind_ == StreamTransportKind::Udp ? AudioSourceType::ExternalUdp : AudioSourceType::ExternalI2S;
}

bool ExternalStreamSource::isKindAvailable(StreamTransportKind kind) const {
  switch (kind) {
    case StreamTransportKind::I2s:
      return i2s_transport_.isAvailable();
    case StreamTransportKind::Udp:
      // Lazily initialized; report available until an init attempt fails.
      return udp_transport_.isAvailable();
    default:
      return false;
  }
}

void ExternalStreamSource::debugPrint() const {
#if defined(ESP_PLATFORM)
  Serial.printf("[STREAM] kind=%s monitoring=%d ring=%u primed=%d playing=%d rx_peak=%d out_peak=%d\n",
                transport_kind_ == StreamTransportKind::Udp ? "udp" : "i2s", monitoring_ ? 1 : 0,
                static_cast<unsigned>(ring_count_), primed_ ? 1 : 0,
                M5.Speaker.isPlaying(SynthConfig::audio.audio_channel), static_cast<int>(rx_peak_),
                static_cast<int>(out_peak_));
  rx_peak_ = 0;
  out_peak_ = 0;
  udp_transport_.debugPrint();
#endif
}

void ExternalStreamSource::setTransportKind(StreamTransportKind kind) {
  if (transport_kind_ == kind) {
    return;
  }
#if defined(ESP_PLATFORM)
  const bool was_monitoring = monitoring_;
  if (was_monitoring) {
    stopMonitor();
  }
  transport_kind_ = kind;
  if (was_monitoring) {
    startMonitor();
  }
#else
  transport_kind_ = kind;
#endif
}

#if defined(ESP_PLATFORM)

StreamTransport& ExternalStreamSource::activeTransport() {
  return transport_kind_ == StreamTransportKind::Udp ? static_cast<StreamTransport&>(udp_transport_)
                                                     : static_cast<StreamTransport&>(i2s_transport_);
}

bool ExternalStreamSource::startMonitor() {
  if (monitoring_) {
    return true;
  }
  if (!activeTransport().start()) {
    return false;
  }

  setVoiceLevel(0, volume_, Waveform::Sine);
  effects_.reset();

  monitoring_ = true;
  BaseType_t result = xTaskCreate(monitorTaskEntry, "external_stream_monitor", 4096, this, 3, &monitor_task_);
  if (result != pdPASS) {
    monitoring_ = false;
    activeTransport().stop();
    monitor_task_ = nullptr;
    return false;
  }

  return true;
}

void ExternalStreamSource::stopMonitor() {
  monitoring_ = false;
  for (int i = 0; i < 50 && monitor_task_ != nullptr; ++i) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  M5.Speaker.stop(SynthConfig::audio.audio_channel);
  activeTransport().stop();
}

void ExternalStreamSource::monitorTaskEntry(void* arg) {
  auto* self = static_cast<ExternalStreamSource*>(arg);
  self->monitorTask();
}

void ExternalStreamSource::ringPush(std::int16_t sample) {
  if (ring_count_ >= kRingCapacity) {
    // Sender clock is ahead of the speaker clock: drop the oldest sample.
    ring_head_ = (ring_head_ + 1) % kRingCapacity;
    --ring_count_;
  }
  ring_[(ring_head_ + ring_count_) % kRingCapacity] = sample;
  ++ring_count_;
}

void ExternalStreamSource::ringPopChunk(std::int16_t* dest, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    dest[i] = ring_[ring_head_];
    ring_head_ = (ring_head_ + 1) % kRingCapacity;
  }
  ring_count_ -= count;
}

void ExternalStreamSource::monitorTask() {
  ring_head_ = 0;
  ring_count_ = 0;
  play_index_ = 0;
  primed_ = false;
  last_gate_ = false;
  stream_envelope_.reset();
  constexpr float kSampleDeltaSeconds = 1.0f / static_cast<float>(SynthConfig::audio.external_i2s_sample_rate);
  StreamTransport& transport = activeTransport();

  while (monitoring_) {
    // 1) Pull frames from the transport (paced by the sender's clock).
    const std::size_t frames = transport.read(read_buffer_.data(), read_buffer_.size(), 40);
    for (std::size_t i = 0; i < frames; ++i) {
      const std::int16_t sample = read_buffer_[i];
      const std::int16_t magnitude = sample < 0 ? -sample : sample;
      if (magnitude > rx_peak_) {
        rx_peak_ = magnitude;
      }
      ringPush(sample);
    }

    // 2) Feed the speaker from the ring. Wait for prefill before (re)starting
    //    so scheduling jitter does not turn into rapid-fire gaps.
    if (!primed_ && ring_count_ >= kPrefillFrames) {
      primed_ = true;
    }
    if (primed_) {
      while (ring_count_ >= kPlayChunkFrames &&
             M5.Speaker.isPlaying(SynthConfig::audio.audio_channel) < 2) {
        auto& play_buffer = play_buffers_[play_index_];
        ringPopChunk(play_buffer.data(), kPlayChunkFrames);

        // Amp envelope driven by the sender's key/gate state. The incoming
        // tone is steady-state, so applying the gate at playback time (not
        // capture time) has no audible skew.
        const bool gate = transport.gate();
        if (gate != last_gate_) {
          last_gate_ = gate;
          if (gate) {
            stream_envelope_.noteOn();
          } else {
            stream_envelope_.noteOff();
          }
        }
        for (std::size_t i = 0; i < kPlayChunkFrames; ++i) {
          const float env = stream_envelope_.process(kSampleDeltaSeconds);
          play_buffer[i] = static_cast<std::int16_t>(static_cast<float>(play_buffer[i]) * env);
        }

        effects_.process(play_buffer.data(), kPlayChunkFrames);
        for (std::size_t i = 0; i < kPlayChunkFrames; ++i) {
          const std::int16_t magnitude = play_buffer[i] < 0 ? -play_buffer[i] : play_buffer[i];
          if (magnitude > out_peak_) {
            out_peak_ = magnitude;
          }
        }
        M5.Speaker.playRaw(play_buffer.data(), kPlayChunkFrames, SynthConfig::audio.external_i2s_sample_rate, false,
                           1, SynthConfig::audio.audio_channel, false);
        play_index_ = (play_index_ + 1) % kPlayBufferCount;
      }
      if (ring_count_ < kPlayChunkFrames &&
          M5.Speaker.isPlaying(SynthConfig::audio.audio_channel) == 0) {
        // Underrun: sender clock is behind. Re-prime to rebuild headroom.
        primed_ = false;
      }
    }
  }

  monitor_task_ = nullptr;
  vTaskDelete(nullptr);
}

#endif
