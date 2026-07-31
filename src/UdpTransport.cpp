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

std::size_t UdpTransport::read(std::int16_t* dest, std::size_t max_frames, std::uint32_t timeout_ms) {
  if (!listening_) {
    return 0;
  }
  const std::uint32_t deadline = millis() + timeout_ms;
  for (;;) {
    const int packet_size = udp_.parsePacket();
    if (packet_size > static_cast<int>(kHeaderBytes)) {
      const int to_read = std::min(packet_size, static_cast<int>(sizeof(packet_buffer_)));
      const int got = udp_.read(packet_buffer_, to_read);
      if (got > static_cast<int>(kHeaderBytes)) {
        std::uint32_t sequence = 0;
        std::memcpy(&sequence, packet_buffer_, sizeof(sequence));
        last_sequence_ = sequence;
        gate_ = packet_buffer_[4] != 0;
        const std::size_t frame_count =
            std::min((static_cast<std::size_t>(got) - kHeaderBytes) / 2, max_frames);
        std::memcpy(dest, packet_buffer_ + kHeaderBytes, frame_count * 2);
        ++packets_received_;
        frames_received_ += frame_count;
        return frame_count;
      }
    }
    if (static_cast<std::int32_t>(millis() - deadline) >= 0) {
      ++empty_polls_;
      return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool UdpTransport::gate() const {
  return gate_;
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
}

#else

bool UdpTransport::begin() { return false; }
bool UdpTransport::start() { return false; }
void UdpTransport::stop() {}
std::size_t UdpTransport::read(std::int16_t*, std::size_t, std::uint32_t) { return 0; }
bool UdpTransport::gate() const { return false; }
bool UdpTransport::isAvailable() const { return false; }
void UdpTransport::debugPrint() const {}

#endif
