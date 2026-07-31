#pragma once

#include "StreamTransport.h"

#if defined(ESP_PLATFORM)
#if __has_include(<WiFi.h>)
#include <WiFi.h>
#include <WiFiUdp.h>
#define UDP_TRANSPORT_SUPPORTED 1
#endif
#endif

#include <cstddef>
#include <cstdint>

// WiFi UDP transport. Tab5 acts as SoftAP so the sender (AtomS3) can connect
// directly without a router. Packets carry an 8-byte header (uint32 sequence,
// uint8 gate, 3 reserved bytes) followed by 16bit mono LE samples.
class UdpTransport : public StreamTransport {
 public:
  static constexpr const char* kApSsid = "Tab5Synth";
  static constexpr const char* kApPassword = "tab5synth";
  static constexpr std::uint16_t kPort = 5005;
  static constexpr std::size_t kMaxPacketFrames = 160;  // 10ms @ 16kHz
  static constexpr std::size_t kHeaderBytes = 8;        // seq(4) + gate(1) + reserved(3)

  bool begin() override;
  bool start() override;
  void stop() override;
  std::size_t read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) override;
  bool gate() const override;
  bool isAvailable() const override;
  void debugPrint() const;

 private:
#if defined(UDP_TRANSPORT_SUPPORTED)
  WiFiUDP udp_;
  std::uint8_t packet_buffer_[kHeaderBytes + kMaxPacketFrames * 2] = {};
  std::uint32_t last_sequence_ = 0;
  bool gate_ = false;
  std::uint32_t packets_received_ = 0;
  std::uint32_t frames_received_ = 0;
  std::uint32_t empty_polls_ = 0;
  bool ap_started_ = false;
  bool listening_ = false;
  bool init_failed_ = false;
#endif
};
