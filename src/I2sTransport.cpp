#include "I2sTransport.h"

#include "AudioBusConfig.h"

#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

bool I2sTransport::begin() {
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
  std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
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

bool I2sTransport::start() {
  if (!begin()) {
    return false;
  }
  if (enabled_) {
    return true;
  }
  if (i2s_channel_enable(rx_handle_) != ESP_OK) {
    return false;
  }
  enabled_ = true;
  return true;
}

void I2sTransport::stop() {
  if (rx_handle_ != nullptr && enabled_) {
    i2s_channel_disable(rx_handle_);
    enabled_ = false;
  }
}

std::size_t I2sTransport::read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) {
  if (rx_handle_ == nullptr || !enabled_) {
    return 0;
  }
  const std::size_t frames_to_read = std::min(max_frames, kBufferFrames);
  std::size_t bytes_read = 0;
  const esp_err_t result =
      i2s_channel_read(rx_handle_, rx_buffer_.data(), frames_to_read * kRxWordsPerFrame * sizeof(std::int32_t),
                       &bytes_read, pdMS_TO_TICKS(timeout_ms));
  if (result != ESP_OK || bytes_read == 0) {
    return 0;
  }
  const std::size_t frame_count = (bytes_read / sizeof(std::int32_t)) / kRxWordsPerFrame;
  std::size_t gate_hits = 0;
  for (std::size_t i = 0; i < frame_count; ++i) {
    const std::int32_t left_word = rx_buffer_[i * kRxWordsPerFrame];
    const std::int16_t right = static_cast<std::int16_t>(rx_buffer_[i * kRxWordsPerFrame + 1] >> 16);
    dest[i] = static_cast<std::int16_t>(left_word >> 16);
    if (right > kGateThreshold) {
      ++gate_hits;
    }
  }
  if (frame_count > 0) {
    gate_ = gate_hits * 2 > frame_count;  // majority of the chunk
  }
  return frame_count;
}

bool I2sTransport::gate() const {
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  return gate_;
#else
  return false;
#endif
}

bool I2sTransport::isAvailable() const {
  return rx_handle_ != nullptr;
}

#else

bool I2sTransport::begin() { return false; }
bool I2sTransport::start() { return false; }
void I2sTransport::stop() {}
std::size_t I2sTransport::read(std::int16_t*, std::size_t, std::uint32_t) { return 0; }
bool I2sTransport::gate() const { return false; }
bool I2sTransport::isAvailable() const { return false; }

#endif
