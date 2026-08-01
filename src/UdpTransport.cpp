#include "UdpTransport.h"

#if defined(UDP_TRANSPORT_SUPPORTED)

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>

bool UdpTransport::begin() {
  if (ap_started_) {
    return true;
  }
  if (init_failed_) {
    return false;
  }
  // Lazy init: bringing up WiFi loads the C6 co-processor firmware and takes
  // a moment, so only do it when the UDP source is actually selected.
  // Tab5 routes the C6 co-processor over SDIO2 on these pins (not the
  // EV-board defaults): CLK=12 CMD=13 D0=11 D1=10 D2=9 D3=8 RESET=15.
  Serial.println("[UDP] WiFi.setPins for Tab5 C6...");
  if (!WiFi.setPins(12, 13, 11, 10, 9, 8, 15)) {
    Serial.println("[UDP] WiFi.setPins failed");
  }
  Serial.println("[UDP] WiFi.mode(WIFI_AP)...");
  WiFi.mode(WIFI_AP);
  Serial.println("[UDP] WiFi.softAP()...");
  if (!WiFi.softAP(kApSsid, kApPassword, 6 /*channel*/)) {
    Serial.println("[UDP] softAP start failed");
    init_failed_ = true;
    return false;
  }
  Serial.println("[UDP] softAP call returned");
  // Short-range link: cap TX power to reduce peak current draw (brownout
  // protection on weak USB supplies).
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.printf("[UDP] softAP up ssid=%s ip=%s port=%u txpower=%d\n", kApSsid, WiFi.softAPIP().toString().c_str(),
                static_cast<unsigned>(kPort), static_cast<int>(WiFi.getTxPower()));
  ap_started_ = true;
  return true;
}

bool UdpTransport::start() {
  if (!begin()) {
    return false;
  }
  if (listening_) {
    return true;
  }
  if (udp_.begin(kPort) != 1) {
    Serial.println("[UDP] listen failed");
    return false;
  }
  listening_ = true;
  return true;
}

void UdpTransport::stop() {
  if (listening_) {
    udp_.stop();
    listening_ = false;
  }
  // Keep the AP running: reconnecting the sender takes seconds, and an idle
  // AP does not disturb audio.
}

UdpTransport::Voice* UdpTransport::slotFor(std::uint8_t wire_id) {
  const std::uint32_t now = millis();
  Voice* oldest = nullptr;
  for (auto& voice : voices_) {
    if (voice.bound && voice.wire_id == wire_id) {
      return &voice;
    }
  }
  for (auto& voice : voices_) {
    if (!voice.bound) {
      voice.bound = true;
      voice.wire_id = wire_id;
      voice.head = 0;
      voice.count = 0;
      voice.gate = false;
      voice.last_packet_ms = now;
      return &voice;
    }
    if (oldest == nullptr || static_cast<std::int32_t>(voice.last_packet_ms - oldest->last_packet_ms) < 0) {
      oldest = &voice;
    }
  }
  // All slots busy: steal the stalest one.
  if (oldest != nullptr && now - oldest->last_packet_ms > kVoiceIdleMs) {
    oldest->wire_id = wire_id;
    oldest->head = 0;
    oldest->count = 0;
    oldest->gate = false;
    oldest->last_packet_ms = now;
    return oldest;
  }
  return nullptr;
}

std::size_t UdpTransport::pump(std::uint32_t timeout_ms) {
  if (!listening_) {
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    return 0;
  }
  const std::uint32_t deadline = millis() + timeout_ms;
  std::size_t packets = 0;
  for (;;) {
    const int packet_size = udp_.parsePacket();
    if (packet_size > static_cast<int>(kHeaderBytes)) {
      const int to_read = std::min(packet_size, static_cast<int>(sizeof(packet_buffer_)));
      const int got = udp_.read(packet_buffer_, to_read);
      if (got > static_cast<int>(kHeaderBytes)) {
        std::uint32_t sequence = 0;
        std::memcpy(&sequence, packet_buffer_, sizeof(sequence));
        last_sequence_ = sequence;
        Voice* voice = slotFor(packet_buffer_[5]);
        if (voice != nullptr) {
          voice->gate = packet_buffer_[4] != 0;
          voice->last_packet_ms = millis();
          const std::size_t frame_count = (static_cast<std::size_t>(got) - kHeaderBytes) / 2;
          const std::int16_t* samples = reinterpret_cast<const std::int16_t*>(packet_buffer_ + kHeaderBytes);
          for (std::size_t i = 0; i < frame_count; ++i) {
            if (voice->count >= kVoiceFifoSize) {
              voice->head = (voice->head + 1) % kVoiceFifoSize;  // drop oldest
              --voice->count;
            }
            voice->fifo[(voice->head + voice->count) % kVoiceFifoSize] = samples[i];
            ++voice->count;
          }
          ++packets_received_;
          frames_received_ += frame_count;
          ++packets;
        }
      }
      // Keep draining any queued packets without waiting.
      if (static_cast<std::int32_t>(millis() - deadline) >= 0) {
        return packets;
      }
      continue;
    }
    if (packets > 0 || static_cast<std::int32_t>(millis() - deadline) >= 0) {
      if (packets == 0) {
        ++empty_polls_;
      }
      return packets;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool UdpTransport::voiceInUse(std::size_t voice) const {
  if (voice >= kMaxVoices) {
    return false;
  }
  const Voice& v = voices_[voice];
  if (!v.bound) {
    return false;
  }
  return (millis() - v.last_packet_ms) < kVoiceReleaseMs || v.count > 0;
}

bool UdpTransport::voiceGate(std::size_t voice) const {
  if (voice >= kMaxVoices) {
    return false;
  }
  const Voice& v = voices_[voice];
  return v.bound && v.gate && (millis() - v.last_packet_ms) < kVoiceIdleMs;
}

std::size_t UdpTransport::readVoice(std::size_t voice, std::int16_t* dest, std::size_t max_frames) {
  if (voice >= kMaxVoices) {
    return 0;
  }
  Voice& v = voices_[voice];
  const std::size_t take = std::min(max_frames, v.count);
  for (std::size_t i = 0; i < take; ++i) {
    dest[i] = v.fifo[v.head];
    v.head = (v.head + 1) % kVoiceFifoSize;
  }
  v.count -= take;
  return take;
}

std::size_t UdpTransport::read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) {
  // Legacy single-stream read: pump then drain voice 0.
  pump(timeout_ms);
  return readVoice(0, dest, max_frames);
}

bool UdpTransport::gate() const {
  return voiceGate(0);
}

bool UdpTransport::isAvailable() const {
  return !init_failed_;
}

void UdpTransport::debugPrint() const {
  Serial.printf("[UDPSTAT] ap=%d listen=%d stations=%d pkts=%u frames=%u empty_polls=%u last_seq=%u\n",
                ap_started_ ? 1 : 0, listening_ ? 1 : 0,
                ap_started_ ? WiFi.softAPgetStationNum() : -1, static_cast<unsigned>(packets_received_),
                static_cast<unsigned>(frames_received_), static_cast<unsigned>(empty_polls_),
                static_cast<unsigned>(last_sequence_));
  for (std::size_t i = 0; i < kMaxVoices; ++i) {
    const Voice& v = voices_[i];
    if (v.bound) {
      Serial.printf("[UDPVOICE] slot=%u id=%u gate=%d fifo=%u age_ms=%u\n", static_cast<unsigned>(i),
                    static_cast<unsigned>(v.wire_id), v.gate ? 1 : 0, static_cast<unsigned>(v.count),
                    static_cast<unsigned>(millis() - v.last_packet_ms));
    }
  }
}

#else

bool UdpTransport::begin() { return false; }
bool UdpTransport::start() { return false; }
void UdpTransport::stop() {}
std::size_t UdpTransport::read(std::int16_t*, std::size_t, std::uint32_t) { return 0; }
bool UdpTransport::gate() const { return false; }
bool UdpTransport::isAvailable() const { return false; }
void UdpTransport::debugPrint() const {}
std::size_t UdpTransport::pump(std::uint32_t) { return 0; }
bool UdpTransport::voiceInUse(std::size_t) const { return false; }
bool UdpTransport::voiceGate(std::size_t) const { return false; }
std::size_t UdpTransport::readVoice(std::size_t, std::int16_t*, std::size_t) { return 0; }

#endif
