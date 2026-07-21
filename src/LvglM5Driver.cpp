#include "LvglM5Driver.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include <cstddef>
#include <cstdint>

namespace {

constexpr int kDisplayWidth = 1280;
constexpr int kDisplayHeight = 720;
constexpr int kDrawBufferLines = 32;
constexpr std::size_t kTouchPoints = 5;

struct TouchPointer {
  std::uint8_t id;
  lv_point_t last_point{};
};

TouchPointer touch_pointers[kTouchPoints]{};
lv_display_t* lvgl_display = nullptr;
bool rendering_enabled = true;

void flushDisplay(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) {
  if (!rendering_enabled) {
    lv_display_flush_ready(display);
    return;
  }
  const int width = area->x2 - area->x1 + 1;
  const int height = area->y2 - area->y1 + 1;
  lv_draw_sw_rgb565_swap(pixels, static_cast<std::uint32_t>(width * height));
  M5.Display.pushImage(area->x1, area->y1, width, height, reinterpret_cast<const std::uint16_t*>(pixels));
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
  auto* pointer = static_cast<TouchPointer*>(lv_indev_get_user_data(indev));
  const std::size_t count = M5.Touch.getCount();
  for (std::size_t i = 0; i < count; ++i) {
    const auto& raw = M5.Touch.getTouchPointRaw(i);
    if (raw.id != pointer->id) {
      continue;
    }
    const auto& touch = M5.Touch.getDetail(i);
    if (touch.isPressed() || touch.wasPressed() || touch.isHolding()) {
      pointer->last_point.x = static_cast<lv_coord_t>(touch.x);
      pointer->last_point.y = static_cast<lv_coord_t>(touch.y);
      data->point = pointer->last_point;
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
  }

  data->point = pointer->last_point;
  data->state = LV_INDEV_STATE_RELEASED;
}

std::uint32_t tickMilliseconds() {
  return millis();
}

}  // namespace

bool LvglM5Driver::begin() {
  lv_init();
  lv_tick_set_cb(tickMilliseconds);

  auto* draw_buffer = static_cast<std::uint16_t*>(
      heap_caps_malloc(kDisplayWidth * kDrawBufferLines * sizeof(std::uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (draw_buffer == nullptr) {
    draw_buffer = static_cast<std::uint16_t*>(
        heap_caps_malloc(kDisplayWidth * kDrawBufferLines * sizeof(std::uint16_t), MALLOC_CAP_8BIT));
  }
  if (draw_buffer == nullptr) {
    return false;
  }

  lvgl_display = lv_display_create(kDisplayWidth, kDisplayHeight);
  lv_display_set_flush_cb(lvgl_display, flushDisplay);
  lv_display_set_buffers(lvgl_display, draw_buffer, nullptr,
                         kDisplayWidth * kDrawBufferLines * sizeof(std::uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(lvgl_display);

  for (std::size_t i = 0; i < kTouchPoints; ++i) {
    touch_pointers[i].id = static_cast<std::uint8_t>(i);
    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, readTouch);
    lv_indev_set_user_data(touch, &touch_pointers[i]);
    lv_indev_set_display(touch, lvgl_display);
  }
  return true;
}

void LvglM5Driver::update() {
  lv_timer_t* refresh_timer = lvgl_display == nullptr ? nullptr : lv_display_get_refr_timer(lvgl_display);
  if (!rendering_enabled && refresh_timer != nullptr) {
    lv_timer_pause(refresh_timer);
  }
  lv_timer_handler();
  // Invalidating an LVGL object automatically resumes this timer. Keep it
  // stopped while audio is active so no hidden PSRAM rendering can run.
  if (!rendering_enabled && refresh_timer != nullptr) {
    lv_timer_pause(refresh_timer);
  }
}

void LvglM5Driver::renderNow() {
  if (lvgl_display == nullptr) {
    return;
  }
  // Keep the refresh timer paused during notes, but permit one synchronous
  // transfer for the small region invalidated by a key-state change.
  const bool was_enabled = rendering_enabled;
  rendering_enabled = true;
  lv_refr_now(lvgl_display);
  rendering_enabled = was_enabled;
}

void LvglM5Driver::setRenderingEnabled(bool enabled) {
  if (lvgl_display == nullptr) {
    return;
  }
  lv_timer_t* refresh_timer = lv_display_get_refr_timer(lvgl_display);
  if (enabled) {
    rendering_enabled = true;
    lv_timer_resume(refresh_timer);
  } else {
    rendering_enabled = false;
    lv_timer_pause(refresh_timer);
  }
}
