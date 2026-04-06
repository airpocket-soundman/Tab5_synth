#pragma once

#include "AudioBusConfig.h"
#include "AudioSource.h"
#include "SynthConfig.h"

#if defined(ESP_PLATFORM)
#if __has_include(<driver/i2s_std.h>)
#include <driver/i2s_std.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>

class ExternalI2SSource : public AudioSource {
 public:
  bool begin() override;
  bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) override;
  void noteOff(std::size_t voice_index) override;
  void noteOffAll() override;
  void setVolume(float volume) override;
  bool isAvailable() const override;
  AudioSourceType type() const override;

 private:
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  static constexpr std::size_t kBufferFrames = SynthConfig::audio.external_i2s_buffer_frames;
  static constexpr std::size_t kBufferCount = SynthConfig::audio.external_i2s_buffer_count;

  static void monitorTaskEntry(void* arg);
  bool initReceiver();
  bool startMonitor();
  void stopMonitor();
  void monitorTask();

  i2s_chan_handle_t rx_handle_ = nullptr;
  TaskHandle_t monitor_task_ = nullptr;
  std::array<std::array<std::int16_t, kBufferFrames>, kBufferCount> buffers_{};
#endif

  bool initialized_ = false;
  bool monitoring_ = false;
  float volume_ = SynthConfig::audio.default_volume;
};
