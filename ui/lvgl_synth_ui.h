#ifndef LVGL_SYNTH_UI_H
#define LVGL_SYNTH_UI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LVGL_SYNTH_THEME_RETRO_WOOD = 0,
    LVGL_SYNTH_THEME_METAL = 1,
    LVGL_SYNTH_THEME_NEON = 2,
} lvgl_synth_theme_t;

enum {
    LVGL_SYNTH_TARGET_COUNT = 17,
    LVGL_SYNTH_EFFECT_COUNT = 4,
};

typedef struct {
    uint8_t values[LVGL_SYNTH_TARGET_COUNT];
    uint8_t lfo_rate[LVGL_SYNTH_TARGET_COUNT];
    uint8_t lfo_depth[LVGL_SYNTH_TARGET_COUNT];
    uint8_t lfo_wave[LVGL_SYNTH_TARGET_COUNT];
    bool lfo_enabled[LVGL_SYNTH_TARGET_COUNT];
    bool effect_enabled[LVGL_SYNTH_EFFECT_COUNT];
    uint8_t source;
    uint8_t mode;
    uint8_t timbre;
} lvgl_synth_state_t;

typedef struct {
    void (*state_changed)(void *user_data);
    void (*note_changed)(float midi_note, bool pressed, void *user_data);
    void (*xy_note_changed)(float midi_note, bool pressed, void *user_data);
    void (*mic_recording_changed)(bool recording, void *user_data);
    void *user_data;
} lvgl_synth_callbacks_t;

void lvgl_synth_ui_set_theme(lvgl_synth_theme_t theme);
void lvgl_synth_ui_set_callbacks(const lvgl_synth_callbacks_t *callbacks);
void lvgl_synth_ui_get_state(lvgl_synth_state_t *snapshot);
void lvgl_synth_ui_create(void);
void lvgl_synth_ui_invalidate_performance(void);
void lvgl_synth_ui_invalidate_dirty_keys(void);
void lvgl_synth_ui_invalidate_dirty_xy(void);

#ifdef __cplusplus
}
#endif

#endif
