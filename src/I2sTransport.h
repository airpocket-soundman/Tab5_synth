#pragma once

#include "StreamTransport.h"
#include "SynthConfig.h"

#if defined(ESP_PLATFORM)
#if __has_include(<driver/i2s_std.h>)
#include <driver/i2s_std.h>
#endif
#endif

#include <array>
#include <cstddef>
#include <cstdint>

// I2S slave RX transport. Receives 32bit stereo slots from the sender
// (AtomS3 master TX) and converts them to 16bit mono frames.
class I2sTransport : public StreamTransport {
 public:
  bool begin() override;
  bool start() override;
  void stop() override;
  std::size_t read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) override;
  bool gate() const override;
  bool isAvailable() const override;

 private:
#if defined(ESP_PLATFORM) && __has_include(<driver/i2s_std.h>)
  static constexpr std::size_t kBufferFrames = SynthConfig::audio.external_i2s_buffer_frames;
  static constexpr std::size_t kRxWordsPerFrame = 2;  // stereo interleaved
  // Right slot carries the sender's key state (0x7FFF = pressed).
  static constexpr std::int16_t kGateThreshold = 1000;

  i2s_chan_handle_t rx_handle_ = nullptr;
  std::array<std::int32_t, kBufferFrames * kRxWordsPerFrame> rx_buffer_{};
  bool enabled_ = false;
  bool gate_ = false;
#endif
};
