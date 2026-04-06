#include "ExternalI2SSource.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>

bool ExternalI2SSource::begin() {
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  initialized_ = initReceiver();
  setVolume(volume_);
  return initialized_;
#else
  initialized_ = false;
  return false;
#endif
}

bool ExternalI2SSource::noteOn(std::size_t /*voice_index*/, float /*note_value*/, float /*frequency*/, Waveform /*waveform*/) {
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  return startMonitor();
#else
  return false;
#endif
}

void ExternalI2SSource::noteOff(std::size_t /*voice_index*/) {
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  stopMonitor();
#endif
}

void ExternalI2SSource::noteOffAll() {
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  stopMonitor();
#endif
}

void ExternalI2SSource::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    M5.Speaker.setChannelVolume(SynthConfig::audio.audio_channel + static_cast<int>(i),
                                static_cast<std::uint8_t>(std::lround(volume_ * 255.0f)));
  }
}

bool ExternalI2SSource::isAvailable() const {
  return initialized_;
}

AudioSourceType ExternalI2SSource::type() const {
  return AudioSourceType::ExternalI2S;
}

#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)

bool ExternalI2SSource::initReceiver() {
  if (rx_handle_ != nullptr) {
    return true;
  }

  i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
  channel_cfg.auto_clear = true;
  if (i2s_new_channel(&channel_cfg, nullptr, &rx_handle_) != ESP_OK) {
    rx_handle_ = nullptr;
    return false;
  }

  i2s_std_config_t std_cfg{};
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SynthConfig::audio.external_i2s_sample_rate);
  std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.bclk = static_cast<gpio_num_t>(AudioBusConfig::ExternalI2SPins::bclk);
  std_cfg.gpio_cfg.ws = static_cast<gpio_num_t>(AudioBusConfig::ExternalI2SPins::ws);
  std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.din = static_cast<gpio_num_t>(AudioBusConfig::ExternalI2SPins::din);
  std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.ws_inv = false;

  if (i2s_channel_init_std_mode(rx_handle_, &std_cfg) != ESP_OK) {
    i2s_del_channel(rx_handle_);
    rx_handle_ = nullptr;
    return false;
  }

  return true;
}

bool ExternalI2SSource::startMonitor() {
  if (monitoring_) {
    return true;
  }
  if (!initReceiver()) {
    initialized_ = false;
    return false;
  }

  setVolume(volume_);
  if (i2s_channel_enable(rx_handle_) != ESP_OK) {
    return false;
  }

  monitoring_ = true;
  BaseType_t result = xTaskCreate(monitorTaskEntry, "external_i2s_monitor", 4096, this, 3, &monitor_task_);
  if (result != pdPASS) {
    monitoring_ = false;
    i2s_channel_disable(rx_handle_);
    monitor_task_ = nullptr;
    return false;
  }

  return true;
}

void ExternalI2SSource::stopMonitor() {
  monitoring_ = false;
  for (int i = 0; i < 50 && monitor_task_ != nullptr; ++i) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  M5.Speaker.stop(SynthConfig::audio.audio_channel);
  if (rx_handle_ != nullptr) {
    i2s_channel_disable(rx_handle_);
  }
}

void ExternalI2SSource::monitorTaskEntry(void* arg) {
  auto* self = static_cast<ExternalI2SSource*>(arg);
  self->monitorTask();
}

void ExternalI2SSource::monitorTask() {
  std::size_t buffer_index = 0;

  while (monitoring_) {
    auto& buffer = buffers_[buffer_index];
    std::size_t bytes_read = 0;
    esp_err_t result = i2s_channel_read(rx_handle_, buffer.data(), buffer.size() * sizeof(std::int16_t),
                                        &bytes_read, pdMS_TO_TICKS(40));
    if (result != ESP_OK || bytes_read == 0) {
      continue;
    }

    const std::size_t sample_count = bytes_read / sizeof(std::int16_t);
    if (sample_count == 0) {
      continue;
    }

    while (monitoring_ && M5.Speaker.isPlaying(SynthConfig::audio.audio_channel) >= 2) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!monitoring_) {
      break;
    }

    M5.Speaker.playRaw(buffer.data(), sample_count, SynthConfig::audio.external_i2s_sample_rate, false, 1,
                       SynthConfig::audio.audio_channel, false);

    buffer_index = (buffer_index + 1) % kBufferCount;
  }

  monitor_task_ = nullptr;
  vTaskDelete(nullptr);
}

#endif
