#include <emscripten.h>
#include <SDL.h>

#include "lvgl.h"
#include "lvgl_synth_ui.h"

enum {
    DISPLAY_WIDTH = 1280,
    DISPLAY_HEIGHT = 720,
};

EM_JS(int, requested_theme, (), {
    const params = new URLSearchParams(window.location.search);
    if(window.location.pathname.endsWith("/cyberdeck.html") ||
       params.get("theme") === "cyberdeck") return 2;
    if(window.location.pathname.endsWith("/metal.html") ||
       params.get("theme") === "metal") return 1;
    return 0;
});

static void app_loop(void *unused)
{
    (void)unused;
    lv_timer_handler();
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(SDL_GetTicks);

    lv_display_t *display = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_default(display);
    lv_sdl_window_set_title(display, "Tab5 Synth LVGL Preview");

    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, display);

    lvgl_synth_ui_set_theme((lvgl_synth_theme_t)requested_theme());
    lvgl_synth_ui_create();
    emscripten_set_main_loop_arg(app_loop, NULL, 0, true);
    return 0;
}
