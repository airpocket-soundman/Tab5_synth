#pragma once

namespace AudioBusConfig {

struct OnboardMicPins {
  static constexpr int mclk = 30;
  static constexpr int bclk = 27;
  static constexpr int data = 28;
  static constexpr int ws = 29;
  static constexpr int i2c_scl = 32;
  static constexpr int i2c_sda = 31;
};

struct ExternalI2SPins {
  static constexpr int bclk = 47;
  static constexpr int ws = 2;
  static constexpr int din = 3;
  static constexpr int mclk = 4;
};

}  // namespace AudioBusConfig
