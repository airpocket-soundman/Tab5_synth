#include "ParameterIcon.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr std::uint32_t kBackgroundColor = 0x000000;
constexpr std::uint32_t kActiveFillColor = 0x4CC9F0;
constexpr std::uint32_t kInactiveFillColor = 0x16232C;
constexpr std::uint32_t kActiveTextColor = 0xDDF8FF;
constexpr std::uint32_t kInactiveTextColor = 0x697581;
constexpr std::uint32_t kBorderColor = 0xF4F4F4;
constexpr int kBorderRadius = 12;
constexpr int kBorderInset = 1;

}

void ParameterIcon::begin(const Rect& bounds) {
  bounds_ = bounds;
  sprite_.setColorDepth(16);
  sprite_.createSprite(bounds_.w, bounds_.h);
  initialized_ = true;
}

void ParameterIcon::drawBar(const char* label, float value, bool selected) {
  if (!initialized_) {
    return;
  }

  const float clamped = std::clamp(value, 0.0f, 1.0f);
  const int inner_w = std::max(0, bounds_.w - (kBorderInset * 2));
  const int inner_h = std::max(0, bounds_.h - (kBorderInset * 2));
  const int active_height = static_cast<int>(std::round(inner_h * clamped));
  const auto fill_color = selected ? kActiveFillColor : kInactiveFillColor;

  sprite_.fillScreen(kBackgroundColor);
  if (active_height > 0 && inner_w > 0 && inner_h > 0) {
    const int fill_y = bounds_.h - kBorderInset - active_height;
    const int fill_radius = std::max(1, std::min(kBorderRadius - 2, active_height / 2));
    sprite_.fillRoundRect(kBorderInset, fill_y, inner_w, active_height, fill_radius, fill_color);
  }

  sprite_.drawRoundRect(0, 0, bounds_.w, bounds_.h, kBorderRadius, kBorderColor);
  sprite_.setTextDatum(middle_center);
  sprite_.setTextColor(selected ? kActiveTextColor : kInactiveTextColor, kBackgroundColor);
  sprite_.setTextSize(2);
  sprite_.drawString(label, bounds_.w / 2, bounds_.h / 2);
  sprite_.setTextDatum(top_left);
  sprite_.setTextSize(1);
  sprite_.pushSprite(bounds_.x, bounds_.y);
}

bool ParameterIcon::contains(int x, int y) const {
  return bounds_.contains(x, y);
}
