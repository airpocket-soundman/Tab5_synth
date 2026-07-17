#ifndef LVGL_SYNTH_UI_H
#define LVGL_SYNTH_UI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LVGL_SYNTH_THEME_RETRO_WOOD = 0,
    LVGL_SYNTH_THEME_METAL = 1,
    LVGL_SYNTH_THEME_CYBERDECK = 2,
} lvgl_synth_theme_t;

void lvgl_synth_ui_set_theme(lvgl_synth_theme_t theme);
void lvgl_synth_ui_create(void);

#ifdef __cplusplus
}
#endif

#endif
