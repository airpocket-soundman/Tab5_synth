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

// WiFi UDP transport. Tab5 acts as SoftAP so senders can connect directly
// without a router. Packets carry an 8-byte header (uint32 sequence, uint8
// gate, uint8 voice_id, 2 reserved bytes) followed by 16bit mono LE samples.
// Packets are demultiplexed by voice_id into independent voice streams so
// overlapping grains / multiple senders play polyphonically.
class UdpTransport : public StreamTransport {
 public:
  static constexpr const char* kApSsid = "Tab5Synth";
  static constexpr const char* kApPassword = "tab5synth";
  static constexpr std::uint16_t kPort = 5005;
  static constexpr std::size_t kMaxPacketFrames = 160;  // 10ms @ 16kHz
  static constexpr std::size_t kHeaderBytes = 8;        // seq(4) + gate(1) + voice(1) + reserved(2)
  static constexpr std::size_t kMaxVoices = 4;
  static constexpr std::size_t kVoiceFifoSize = 2048;      // 128ms per voice
  static constexpr std::uint32_t kVoiceIdleMs = 400;       // gate off when stream stalls
  static constexpr std::uint32_t kVoiceReleaseMs = 1500;   // unbind slot

  bool begin() override;
  bool start() override;
  void stop() override;
  std::size_t read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) override;
  bool gate() const override;
  bool isAvailable() const override;
  void debugPrint() const;

  // Polyphonic access: pump() ingests pending packets (waiting up to
  // timeout for the first one), then per-voice reads are non-blocking.
  std::size_t pump(std::uint32_t timeout_ms);
  std::size_t voiceCount() const { return kMaxVoices; }
  bool voiceInUse(std::size_t voice) const;
  bool voiceGate(std::size_t voice) const;
  std::size_t readVoice(std::size_t voice, std::int16_t* dest, std::size_t max_frames);

 private:
#if defined(UDP_TRANSPORT_SUPPORTED)
  struct Voice {
    bool bound = false;
    std::uint8_t wire_id = 0;
    bool gate = false;
    std::uint32_t last_packet_ms = 0;
    std::int16_t fifo[kVoiceFifoSize] = {};
    std::size_t head = 0;
    std::size_t count = 0;
  };

  Voice* slotFor(std::uint8_t wire_id);

  WiFiUDP udp_;
  std::uint8_t packet_buffer_[kHeaderBytes + kMaxPacketFrames * 2] = {};
  std::uint32_t last_sequence_ = 0;
  Voice voices_[kMaxVoices] = {};
  std::uint32_t packets_received_ = 0;
  std::uint32_t frames_received_ = 0;
  std::uint32_t empty_polls_ = 0;
  bool ap_started_ = false;
  bool listening_ = false;
  bool init_failed_ = false;
#endif
};
