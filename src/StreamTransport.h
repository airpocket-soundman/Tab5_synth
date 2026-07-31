#pragma once

#include <cstddef>
#include <cstdint>

// Abstraction over the byte-transport that delivers external audio to the
// synth. Implementations normalize their input to 16bit/mono frames at
// SynthConfig::audio.external_i2s_sample_rate so the stream source (ring
// buffer, effects, playback) stays transport-agnostic.
class StreamTransport {
 public:
  virtual ~StreamTransport() = default;

  // One-time hardware/network initialization. Safe to call repeatedly.
  virtual bool begin() = 0;
  // Enable reception (called when the source starts monitoring).
  virtual bool start() = 0;
  // Pause reception.
  virtual void stop() = 0;
  // Read up to max_frames mono samples, waiting at most timeout_ms.
  // Returns the number of frames written to dest (0 on timeout).
  virtual std::size_t read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) = 0;
  // Key/gate state transmitted alongside the audio (sender button). Updated
  // by read(); drives the amp envelope on the receiver.
  virtual bool gate() const = 0;
  virtual bool isAvailable() const = 0;
};
