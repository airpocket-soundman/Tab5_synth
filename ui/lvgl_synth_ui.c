#include "lvgl_synth_ui.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    C_WOOD = 0x5a2d18,
    C_BG = 0x120e0b,
    C_PANEL = 0x181916,
    C_PANEL_2 = 0x282923,
    C_EDGE = 0x766747,
    C_LINE = 0x555345,
    C_TEXT = 0xf0e4c4,
    C_MUTED = 0xaaa389,
    C_AMBER = 0xe0a43b,
    C_CYAN = 0x4fb8b0,
    C_LIME = 0xa7ad50,
    C_RED = 0xbf4838,
    C_BRASS = 0xa77a35,
    NEON_CYAN = 0x28e7f2,
    NEON_MAGENTA = 0xf04fbf,
    NEON_LIME = 0xb8f34a,
    NEON_AMBER = 0xffb52e,
    NEON_RED = 0xff4057,
};

enum { PAGE_AMP, PAGE_FX, PAGE_LFO, PAGE_BANK, PAGE_COUNT };
enum { SOURCE_SINE, SOURCE_SAW, SOURCE_SQUARE, SOURCE_TRIANGLE, SOURCE_MIC, SOURCE_I2S, SOURCE_UDP, SOURCE_COUNT };
enum { FX_DELAY, FX_CHORUS, FX_DRIVE, FX_CRUSH, FX_COUNT };
enum { EDIT_BASE, EDIT_LFO_RATE, EDIT_LFO_DEPTH, EDIT_LFO_WAVE };
enum { AMP_ENV_COLS = 40, AMP_ENV_ROWS = 8 };

enum {
    T_VOL, T_ATK, T_DEC, T_SUS, T_REL,
    T_DELAY_TIME, T_DELAY_FBK, T_DELAY_MIX,
    T_CHORUS_RATE, T_CHORUS_DEP, T_CHORUS_MIX,
    T_DRIVE_DRV, T_DRIVE_TON, T_DRIVE_MIX,
    T_CRUSH_BITS, T_CRUSH_RATE, T_CRUSH_MIX,
    TARGET_COUNT
};

typedef struct {
    bool enabled;
    uint8_t rate;
    uint8_t depth;
    uint8_t wave;
} lfo_state_t;

typedef struct {
    uint8_t value[TARGET_COUNT];
    lfo_state_t lfo[TARGET_COUNT];
    bool effect_on[FX_COUNT];
    uint8_t source;
    uint8_t mode;
    uint8_t timbre;
} synth_state_t;

typedef struct {
    lv_obj_t *obj;
    lv_obj_t *text;
    lv_obj_t *lamp;
    lv_obj_t *rail;
    int16_t depth;
    int16_t target_depth;
} ui_button_t;

enum { ROTARY_AMP, ROTARY_FX, ROTARY_LFO };

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *rim;
    lv_obj_t *body;
    lv_obj_t *ticks[11];
    lv_obj_t *indicator_groove;
    lv_obj_t *indicator;
    lv_obj_t *value;
    lv_obj_t *caption;
    lv_point_precise_t tick_points[11][2];
    lv_point_precise_t indicator_points[2];
    uint8_t role;
    uint8_t index;
} rotary_knob_t;

typedef struct {
    lv_obj_t *obj;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t press_progress;
    int16_t midi_note;
    bool black;
    bool visual_dirty;
} piano_key_t;

static const char *const target_names[TARGET_COUNT] = {
    "VOL", "ATK", "DEC", "SUS", "REL",
    "DELAY TIME", "DELAY FBK", "DELAY MIX",
    "CHORUS RATE", "CHORUS DEP", "CHORUS MIX",
    "DRIVE DRV", "DRIVE TON", "DRIVE MIX",
    "CRUSH BITS", "CRUSH RATE", "CRUSH MIX"
};

static const char *const short_target_names[TARGET_COUNT] = {
    "VOL", "ATK", "DEC", "SUS", "REL",
    "TIME", "FBK", "MIX", "RATE", "DEP", "MIX",
    "DRV", "TON", "MIX", "BITS", "RATE", "MIX"
};

static const int fx_target_base[FX_COUNT] = {T_DELAY_TIME, T_CHORUS_RATE, T_DRIVE_DRV, T_CRUSH_BITS};

static int lfo_wave_index(uint8_t value)
{
    if(value < 20) return 0;
    if(value < 55) return 1;
    if(value < 85) return 2;
    return 3;
}

static const char *lfo_wave_name(uint8_t value)
{
    static const char *const names[4] = {"SINE", "TRIANGLE", "SQUARE", "RND"};
    return names[lfo_wave_index(value)];
}

static synth_state_t state;
static synth_state_t memories[5];
static bool memories_ready;
static bool state_ready;
static int current_memory;
static int active_preset = -1;
static int current_page = PAGE_AMP;
static int current_target = T_VOL;
static int current_fx = FX_DELAY;
static int edit_mode = EDIT_BASE;
static bool mic_recording;
static bool mic_ready;
static uint32_t mic_started_ms;
static bool refreshing;

static ui_button_t source_buttons[SOURCE_COUNT];
static ui_button_t page_buttons[PAGE_COUNT];
static ui_button_t mode_buttons[2];
static ui_button_t effect_buttons[FX_COUNT];
static ui_button_t bank_buttons[16];
static lv_obj_t *pages[PAGE_COUNT];
static rotary_knob_t amp_knobs[5];
static rotary_knob_t fx_knobs[3];
static rotary_knob_t lfo_knobs[2];
static ui_button_t lfo_wave_buttons[4];
static lv_obj_t *amp_envelope_module;
static lv_obj_t *amp_envelope_dots[AMP_ENV_COLS][AMP_ENV_ROWS];
static int8_t amp_envelope_active_rows[AMP_ENV_COLS];
static lv_obj_t *amp_stage_labels[4];
static lv_point_precise_t amp_envelope_points[5];
static int16_t amp_envelope_values[4] = {-1, -1, -1, -1};
static int16_t amp_envelope_anim_start[4];
static int16_t amp_envelope_anim_target[4];
static lv_obj_t *xy_marker;
static lv_obj_t *xy_readout;
static lv_obj_t *xy_pad;
static lv_area_t xy_marker_dirty_area;
static bool xy_marker_visual_dirty;
static bool xy_readout_visual_dirty;
static lv_obj_t *keyboard_panel;
static lv_obj_t *status_label;
static lv_obj_t *source_readout;
static lv_obj_t *target_label;
static lv_obj_t *target_value;
static lv_obj_t *common_slider;
static lv_obj_t *fader_cap;
static int16_t fader_visual_value = -1;
static lv_obj_t *lfo_target_label;
static lv_obj_t *lfo_state_label;
static ui_button_t lfo_power_button;
static bool programmatic_rotary_update;
static bool animate_control_updates;
static piano_key_t white_keys[15];
static piano_key_t black_keys[10];

typedef struct {
    lv_indev_t *indev;
    int16_t midi_note;
} keyboard_gesture_t;

static keyboard_gesture_t keyboard_gestures[5];
static lvgl_synth_theme_t current_theme = LVGL_SYNTH_THEME_RETRO_WOOD;
static lvgl_synth_callbacks_t callbacks;

static void notify_state_changed(void)
{
    if(callbacks.state_changed != NULL) callbacks.state_changed(callbacks.user_data);
}

static void notify_note_changed(float midi_note, bool pressed)
{
    if(callbacks.note_changed != NULL) callbacks.note_changed(midi_note, pressed, callbacks.user_data);
}

static void notify_xy_note_changed(float midi_note, bool pressed)
{
    if(callbacks.xy_note_changed != NULL) callbacks.xy_note_changed(midi_note, pressed, callbacks.user_data);
}

static void notify_mic_recording_changed(bool recording)
{
    if(callbacks.mic_recording_changed != NULL) {
        callbacks.mic_recording_changed(recording, callbacks.user_data);
    }
}

void lvgl_synth_ui_set_callbacks(const lvgl_synth_callbacks_t *new_callbacks)
{
    if(new_callbacks == NULL) memset(&callbacks, 0, sizeof(callbacks));
    else callbacks = *new_callbacks;
}

void lvgl_synth_ui_get_state(lvgl_synth_state_t *snapshot)
{
    int i;
    if(snapshot == NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    memcpy(snapshot->values, state.value, sizeof(state.value));
    memcpy(snapshot->effect_enabled, state.effect_on, sizeof(state.effect_on));
    snapshot->source = state.source;
    snapshot->mode = state.mode;
    snapshot->timbre = state.timbre;
    for(i = 0; i < TARGET_COUNT; ++i) {
        snapshot->lfo_rate[i] = state.lfo[i].rate;
        snapshot->lfo_depth[i] = state.lfo[i].depth;
        snapshot->lfo_wave[i] = state.lfo[i].wave;
        snapshot->lfo_enabled[i] = state.lfo[i].enabled;
    }
}

static bool is_metal_theme(void)
{
    return current_theme == LVGL_SYNTH_THEME_METAL;
}

static bool is_neon_theme(void)
{
    return current_theme == LVGL_SYNTH_THEME_NEON;
}

void lvgl_synth_ui_set_theme(lvgl_synth_theme_t theme)
{
    if(theme == LVGL_SYNTH_THEME_METAL || theme == LVGL_SYNTH_THEME_NEON) {
        current_theme = theme;
    } else {
        current_theme = LVGL_SYNTH_THEME_RETRO_WOOD;
    }
}

static uint32_t metal_color(uint32_t value)
{
    switch(value) {
        case 0x030302: return 0x030405;
        case 0x050504: return 0x06080a;
        case 0x06110f: return 0x061313;
        case 0x070806: return 0x090c0e;
        case 0x080604: return 0x050709;
        case 0x080806: return 0x090b0d;
        case 0x090907: return 0x0b0e10;
        case 0x0b211d: return 0x0a2828;
        case 0x0c0d0b: return 0x0c1013;
        case 0x0d100d: return 0x0e1316;
        case 0x10110f: return 0x11161a;
        case 0x11110f: return 0x12171b;
        case 0x11120f: return 0x13191d;
        case 0x120e0b: return 0x101418;
        case 0x121310: return 0x151a1e;
        case 0x151613: return 0x1b2125;
        case 0x17130e: return 0x151b20;
        case 0x171713: return 0x1c2226;
        case 0x173b35: return 0x174740;
        case 0x181916: return 0x1a2024;
        case 0x27574e: return 0x28675d;
        case 0x282923: return 0x293137;
        case 0x28564e: return 0x29675e;
        case 0x292a24: return 0x2c3439;
        case 0x2a2b25: return 0x2c3439;
        case 0x2cb9a3: return 0x35c8b4;
        case 0x30271c: return 0x353e43;
        case 0x35362f: return 0x3a444a;
        case 0x39372e: return 0x414b50;
        case 0x3b3325: return 0x3a454b;
        case 0x3d4035: return 0x48535a;
        case 0x414036: return 0x4d575d;
        case 0x42d9c1: return 0x4ce1ca;
        case 0x4d9f91: return 0x55ad9f;
        case 0x4fb8b0: return 0x57c4bd;
        case 0x514936: return 0x59656b;
        case 0x554d3e: return 0x606b71;
        case 0x555345: return 0x657178;
        case 0x5a2d18: return 0x252c31;
        case 0x5d5b4c: return 0x6c787e;
        case 0x62e6cf: return 0x6cebd7;
        case 0x6d6554: return 0x7b878d;
        case 0x766747: return 0x7c898f;
        case 0x766e58: return 0x879399;
        case 0x8c672d: return 0x87949a;
        case 0x9e9278: return 0x89959b;
        case 0xa77a35: return 0x8e9ba1;
        case 0xa7ad50: return 0xb1bd58;
        case 0xaaa389: return 0xa6b1b7;
        case 0xbf4838: return 0xc94f43;
        case 0xe0a43b: return 0xe2a63f;
        case 0xe8ddbd: return 0xd7dde0;
        case 0xf0e4c4: return 0xe7edf0;
        case 0xf2e7c8: return 0xdce4e7;
        case 0xfaf0d4: return 0xf1f5f6;
        default: return value;
    }
}

static uint32_t neon_color(uint32_t value)
{
    switch(value) {
        case 0x030302: return 0x020304;
        case 0x050504: return 0x030506;
        case 0x06110f: return 0x031416;
        case 0x070806: return 0x050809;
        case 0x080604: return 0x030405;
        case 0x080806: return 0x06090a;
        case 0x090907: return 0x080c0e;
        case 0x0b211d: return 0x06272a;
        case 0x0c0d0b: return 0x0b1012;
        case 0x0d100d: return 0x071316;
        case 0x10110f: return 0x0a0f11;
        case 0x11110f: return 0x0c1113;
        case 0x11120f: return 0x0d1315;
        case 0x120e0b: return 0x080d0f;
        case 0x121310: return 0x111719;
        case 0x151613: return 0x151c1f;
        case 0x17130e: return 0x12191b;
        case 0x171713: return 0x0d1315;
        case 0x173b35: return 0x15545a;
        case 0x181916: return 0x111719;
        case 0x27574e: return 0x24727a;
        case 0x282923: return 0x202a2d;
        case 0x28564e: return 0x245c61;
        case 0x292a24: return 0x20282b;
        case 0x2a2b25: return 0x171f21;
        case 0x2cb9a3: return 0x30d5d0;
        case 0x30271c: return 0x58351f;
        case 0x35362f: return 0x273236;
        case 0x39372e: return 0x344247;
        case 0x3b3325: return 0x29363a;
        case 0x3d4035: return 0x28464a;
        case 0x414036: return 0x3c4b50;
        case 0x42d9c1: return 0x39e6dd;
        case 0x4d9f91: return 0x55c9c5;
        case 0x4fb8b0: return NEON_CYAN;
        case 0x514936: return 0x48565a;
        case 0x554d3e: return 0x46575d;
        case 0x555345: return 0x42565b;
        case 0x5a2d18: return 0x0a0e10;
        case 0x5d5b4c: return 0x52646a;
        case 0x62e6cf: return 0x78fff0;
        case 0x6d6554: return 0x53676d;
        case 0x766747: return 0x60757b;
        case 0x766e58: return 0x68787d;
        case 0x8c672d: return 0x9b6131;
        case 0x9e9278: return 0x748186;
        case 0xa77a35: return NEON_MAGENTA;
        case 0xa7ad50: return NEON_LIME;
        case 0xaaa389: return 0xa5b4b7;
        case 0xbf4838: return NEON_RED;
        case 0xe0a43b: return NEON_AMBER;
        case 0xe8ddbd: return 0xc8d0cc;
        case 0xf0e4c4: return 0xe6eeee;
        case 0xf2e7c8: return 0xaebbbe;
        case 0xfaf0d4: return 0xf0f7f5;
        default: return value;
    }
}

static lv_color_t col(uint32_t value)
{
    if(current_theme == LVGL_SYNTH_THEME_METAL) return lv_color_hex(metal_color(value));
    if(current_theme == LVGL_SYNTH_THEME_NEON) return lv_color_hex(neon_color(value));
    return lv_color_hex(value);
}

static void no_scroll(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            uint32_t color_value)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, col(color_value), 0);
    return label;
}

static lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t fill)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, col(fill), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(obj);
    return obj;
}

static lv_obj_t *make_neon_rect(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t color, lv_opa_t opacity, int glow_width)
{
    lv_obj_t *outer_glow = make_rect(parent, x - 8, y - 8, w + 16, h + 16, color);
    lv_obj_t *inner_glow = make_rect(parent, x - 4, y - 4, w + 8, h + 8, color);
    lv_obj_t *obj = make_rect(parent, x, y, w, h, color);
    lv_obj_set_style_bg_opa(outer_glow, LV_OPA_10, 0);
    lv_obj_set_style_bg_opa(inner_glow, LV_OPA_30, 0);
    lv_obj_set_style_bg_opa(obj, opacity, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_shadow_width(obj, glow_width + 9, 0);
    lv_obj_set_style_shadow_spread(obj, 2, 0);
    lv_obj_set_style_shadow_ofs_x(obj, 0, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 0, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_80, 0);
    return obj;
}

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 3, 0);
    lv_obj_set_style_bg_color(obj, col(C_PANEL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, col(C_EDGE), 0);
    lv_obj_set_style_shadow_width(obj, 7, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 3, 0);
    lv_obj_set_style_shadow_color(obj, col(0x080604), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    no_scroll(obj);
    if(is_metal_theme()) {
        int grain_y;
        lv_obj_set_style_radius(obj, 2, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x596368), 0);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0xd9ddde), 0);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_border_width(obj, 2, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xd7dee1), 0);
        lv_obj_set_style_outline_width(obj, 1, 0);
        lv_obj_set_style_outline_color(obj, lv_color_hex(0x080b0d), 0);
        lv_obj_set_style_outline_pad(obj, 1, 0);
        lv_obj_set_style_shadow_width(obj, 9, 0);
        lv_obj_set_style_shadow_ofs_y(obj, 4, 0);
        lv_obj_set_style_shadow_color(obj, lv_color_hex(0x020304), 0);
        lv_obj_set_style_shadow_opa(obj, LV_OPA_70, 0);
        for(grain_y = 4; grain_y < h - 4; grain_y += 2) {
            int left = 4 + (grain_y * 23) % 31;
            int right = 5 + (grain_y * 17) % 37;
            int bright = ((grain_y * 13) % 11) < 5;
            lv_obj_t *grain = make_rect(obj, left, grain_y, w - left - right, 1,
                                        bright ? 0xf3f5f5 : 0x303a3f);
            lv_obj_set_style_bg_opa(grain, bright ? LV_OPA_20 : LV_OPA_10, 0);
            if((grain_y % 10) == 4 && w > 240) {
                int streak_x = 22 + (grain_y * 29) % (w - 220);
                int streak_w = 90 + (grain_y * 7) % 120;
                lv_obj_t *streak = make_rect(obj, streak_x, grain_y, streak_w, 1,
                                             (grain_y % 20) == 4 ? 0xffffff : 0x1e282d);
                lv_obj_set_style_bg_opa(streak,
                                        (grain_y % 20) == 4 ? LV_OPA_30 : LV_OPA_10, 0);
            }
        }
        {
            lv_obj_t *top_reflection = make_rect(obj, 5, 3, w - 10, 1, 0xc3ccd0);
            lv_obj_t *bottom_reflection = make_rect(obj, 5, h - 5, w - 10, 1, 0x06090b);
            lv_obj_set_style_bg_opa(top_reflection, LV_OPA_70, 0);
            lv_obj_set_style_bg_opa(bottom_reflection, LV_OPA_40, 0);
        }
    } else if(is_neon_theme()) {
        lv_obj_set_style_radius(obj, 2, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x111719), 0);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x080c0e), 0);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(obj, 2, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_border_opa(obj, LV_OPA_70, 0);
        lv_obj_set_style_outline_width(obj, 2, 0);
        lv_obj_set_style_outline_color(obj, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_outline_opa(obj, LV_OPA_70, 0);
        lv_obj_set_style_outline_pad(obj, 1, 0);
        lv_obj_set_style_shadow_width(obj, 22, 0);
        lv_obj_set_style_shadow_ofs_y(obj, 0, 0);
        lv_obj_set_style_shadow_color(obj, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_shadow_opa(obj, LV_OPA_40, 0);
        {
            make_neon_rect(obj, 18, 5, 80, 2, NEON_CYAN, LV_OPA_COVER, 10);
            make_neon_rect(obj, 102, 5, 34, 2, NEON_MAGENTA, LV_OPA_COVER, 9);
            make_neon_rect(obj, w - 113, h - 7, 54, 2, NEON_LIME, LV_OPA_COVER, 9);
            make_neon_rect(obj, w - 55, h - 7, 36, 2, NEON_AMBER, LV_OPA_COVER, 9);
        }
    }
    return obj;
}

static void add_screws(lv_obj_t *parent, int w, int h)
{
    const int points[4][2] = {{7, 7}, {w - 13, 7}, {7, h - 13}, {w - 13, h - 13}};
    int i;
    for(i = 0; i < 4; ++i) {
        lv_obj_t *screw = make_rect(parent, points[i][0], points[i][1], 6, 6, C_BRASS);
        lv_obj_set_style_radius(screw, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(screw, 1, 0);
        lv_obj_set_style_border_color(screw, col(0x30271c), 0);
        if(is_metal_theme()) {
            lv_obj_t *slot;
            lv_obj_set_style_bg_color(screw, lv_color_hex(0xc3cbd0), 0);
            lv_obj_set_style_bg_grad_color(screw, lv_color_hex(0x59656b), 0);
            lv_obj_set_style_bg_grad_dir(screw, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(screw, lv_color_hex(0x222a2f), 0);
            lv_obj_set_style_shadow_width(screw, 3, 0);
            lv_obj_set_style_shadow_ofs_y(screw, 1, 0);
            lv_obj_set_style_shadow_color(screw, lv_color_hex(0x020304), 0);
            slot = make_rect(screw, 1, 2, 4, 1, 0x151a1e);
            lv_obj_set_style_bg_color(slot, lv_color_hex(0x273036), 0);
        } else if(is_neon_theme()) {
            lv_obj_t *core;
            lv_obj_set_size(screw, 8, 8);
            lv_obj_set_style_bg_color(screw, lv_color_hex(0x273136), 0);
            lv_obj_set_style_bg_grad_color(screw, lv_color_hex(0x090d0f), 0);
            lv_obj_set_style_bg_grad_dir(screw, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(screw, lv_color_hex(0x68777b), 0);
            lv_obj_set_style_shadow_width(screw, 4, 0);
            lv_obj_set_style_shadow_ofs_y(screw, 2, 0);
            lv_obj_set_style_shadow_color(screw, lv_color_hex(0x010203), 0);
            core = make_rect(screw, 3, 3, 2, 2, 0xc06f35);
            lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
        }
    }
}

static void apply_button_depth(ui_button_t *button, int32_t depth)
{
    button->depth = (int16_t)depth;
    lv_obj_set_style_translate_y(button->obj, depth, 0);
    lv_obj_set_style_shadow_width(button->obj, 7 - depth, 0);
    lv_obj_set_style_shadow_ofs_y(button->obj, 4 - depth, 0);
    lv_obj_set_style_shadow_opa(button->obj, depth >= 2 ? LV_OPA_30 : LV_OPA_60, 0);
}

static void button_depth_anim_exec(void *var, int32_t value)
{
    apply_button_depth((ui_button_t *)var, value);
}

static ui_button_t *find_button(lv_obj_t *obj)
{
    int i;

    for(i = 0; i < SOURCE_COUNT; ++i) if(source_buttons[i].obj == obj) return &source_buttons[i];
    for(i = 0; i < PAGE_COUNT; ++i) if(page_buttons[i].obj == obj) return &page_buttons[i];
    for(i = 0; i < 2; ++i) if(mode_buttons[i].obj == obj) return &mode_buttons[i];
    for(i = 0; i < FX_COUNT; ++i) if(effect_buttons[i].obj == obj) return &effect_buttons[i];
    for(i = 0; i < 16; ++i) if(bank_buttons[i].obj == obj) return &bank_buttons[i];
    if(lfo_power_button.obj == obj) return &lfo_power_button;
    return NULL;
}

static void button_press_visual_event(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target_obj(event);
    ui_button_t *button = find_button(obj);

    if(button != NULL) lv_obj_set_style_translate_y(obj, button->depth + 2, LV_STATE_PRESSED);
}

static ui_button_t make_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                               lv_event_cb_t callback, int user_value)
{
    ui_button_t result;
    int lamp_width = w >= 100 ? 24 : 16;
    result.obj = lv_button_create(parent);
    result.depth = 0;
    result.target_depth = -1;
    lv_obj_set_pos(result.obj, x, y);
    lv_obj_set_size(result.obj, w, h);
    lv_obj_set_style_radius(result.obj, 3, 0);
    lv_obj_set_style_bg_color(result.obj, col(0x35362f), 0);
    lv_obj_set_style_bg_grad_color(result.obj, col(0x151613), 0);
    lv_obj_set_style_bg_grad_dir(result.obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(result.obj, 16, 0);
    lv_obj_set_style_bg_grad_stop(result.obj, 220, 0);
    lv_obj_set_style_border_width(result.obj, 2, 0);
    lv_obj_set_style_border_color(result.obj, col(0x5d5b4c), 0);
    lv_obj_set_style_border_opa(result.obj, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(result.obj, 7, 0);
    lv_obj_set_style_shadow_ofs_y(result.obj, 4, 0);
    lv_obj_set_style_shadow_color(result.obj, col(0x050504), 0);
    lv_obj_set_style_shadow_opa(result.obj, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(result.obj, 0, 0);
    lv_obj_set_style_shadow_width(result.obj, 2, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_ofs_y(result.obj, 1, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(result.obj, LV_OPA_20, LV_STATE_PRESSED);
    if(is_metal_theme()) {
        lv_obj_set_style_radius(result.obj, 2, 0);
        lv_obj_set_style_bg_color(result.obj, lv_color_hex(0x56636a), 0);
        lv_obj_set_style_bg_grad_color(result.obj, lv_color_hex(0x20282d), 0);
        lv_obj_set_style_bg_main_stop(result.obj, 24, 0);
        lv_obj_set_style_bg_grad_stop(result.obj, 210, 0);
        lv_obj_set_style_border_color(result.obj, lv_color_hex(0x9aa6ab), 0);
        lv_obj_set_style_outline_width(result.obj, 1, 0);
        lv_obj_set_style_outline_color(result.obj, lv_color_hex(0x0a0e11), 0);
        lv_obj_set_style_outline_pad(result.obj, 1, 0);
        lv_obj_set_style_shadow_width(result.obj, 8, 0);
        lv_obj_set_style_shadow_ofs_y(result.obj, 5, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_radius(result.obj, 2, 0);
        lv_obj_set_style_bg_color(result.obj, lv_color_hex(0x293438), 0);
        lv_obj_set_style_bg_grad_color(result.obj, lv_color_hex(0x0b1012), 0);
        lv_obj_set_style_bg_main_stop(result.obj, 20, 0);
        lv_obj_set_style_bg_grad_stop(result.obj, 205, 0);
        lv_obj_set_style_bg_opa(result.obj, LV_OPA_90, 0);
        lv_obj_set_style_border_color(result.obj, lv_color_hex(0x53666c), 0);
        lv_obj_set_style_outline_width(result.obj, 1, 0);
        lv_obj_set_style_outline_color(result.obj, lv_color_hex(0xb26935), 0);
        lv_obj_set_style_outline_opa(result.obj, LV_OPA_30, 0);
        lv_obj_set_style_outline_pad(result.obj, 1, 0);
        lv_obj_set_style_shadow_width(result.obj, 8, 0);
        lv_obj_set_style_shadow_ofs_y(result.obj, 5, 0);
    }
    no_scroll(result.obj);
    lv_obj_add_event_cb(result.obj, button_press_visual_event, LV_EVENT_PRESSED, NULL);
    if(callback != NULL) {
        lv_obj_add_event_cb(result.obj, callback, LV_EVENT_CLICKED, (void *)(intptr_t)user_value);
    }
    result.text = make_label(result.obj, text, &lv_font_montserrat_12, C_MUTED);
    lv_obj_center(result.text);
    lv_obj_set_style_translate_y(result.text, -2, 0);
    result.lamp = lv_obj_create(result.obj);
    lv_obj_set_pos(result.lamp, (w - lamp_width) / 2, h - 8);
    lv_obj_set_size(result.lamp, lamp_width, 3);
    lv_obj_set_style_radius(result.lamp, 1, 0);
    lv_obj_set_style_bg_color(result.lamp, col(C_LINE), 0);
    lv_obj_set_style_bg_opa(result.lamp, LV_OPA_30, 0);
    lv_obj_set_style_border_width(result.lamp, 1, 0);
    lv_obj_set_style_border_color(result.lamp, col(0x080806), 0);
    lv_obj_set_style_shadow_width(result.lamp, 0, 0);
    lv_obj_set_style_pad_all(result.lamp, 0, 0);
    lv_obj_remove_flag(result.lamp, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(result.lamp);
    result.rail = make_rect(result.obj, 2, 5, 3, h - 12, C_LINE);
    lv_obj_set_style_bg_opa(result.rail, is_neon_theme() ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    return result;
}

static void style_button(ui_button_t *button, bool latched, bool focused, bool lit, uint32_t accent)
{
    int16_t target_depth = latched ? 3 : 0;

    lv_obj_set_style_bg_color(button->obj, col(latched ? 0x2a2b25 : 0x35362f), 0);
    lv_obj_set_style_bg_grad_color(button->obj, col(latched ? 0x11120f : 0x151613), 0);
    lv_obj_set_style_border_color(button->obj, col(focused ? accent : (lit ? accent : 0x5d5b4c)), 0);
    lv_obj_set_style_border_opa(button->obj, focused ? LV_OPA_COVER : (lit ? LV_OPA_70 : LV_OPA_COVER), 0);
    lv_obj_set_style_text_color(button->text, col(focused ? accent : (lit ? C_TEXT : C_MUTED)), 0);
    lv_obj_set_style_bg_color(button->lamp, col(lit ? accent : C_LINE), 0);
    lv_obj_set_style_bg_opa(button->lamp, lit ? LV_OPA_COVER : LV_OPA_20, 0);
    lv_obj_set_style_border_color(button->lamp, col(lit ? C_TEXT : 0x080806), 0);
    lv_obj_set_style_border_opa(button->lamp, lit ? LV_OPA_60 : LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(button->lamp, col(accent), 0);
    lv_obj_set_style_shadow_width(button->lamp, lit ? 7 : 0, 0);
    lv_obj_set_style_shadow_opa(button->lamp, lit ? LV_OPA_60 : LV_OPA_TRANSP, 0);

    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(button->obj,
                                  lv_color_hex(latched ? 0x252d32 : 0x59666d), 0);
        lv_obj_set_style_bg_grad_color(button->obj,
                                       lv_color_hex(latched ? 0x12181c : 0x222a2f), 0);
        lv_obj_set_style_border_color(button->obj,
                                      lit || focused ? col(accent) : lv_color_hex(0x929da2), 0);
        lv_obj_set_style_outline_color(button->obj, lv_color_hex(0x080b0d), 0);
        lv_obj_set_style_text_color(button->text,
                                    lit || focused ? col(accent) : lv_color_hex(0xd4dadd), 0);
        lv_obj_set_style_bg_color(button->lamp,
                                  lit ? col(accent) : lv_color_hex(0x11171a), 0);
        lv_obj_set_style_border_color(button->lamp,
                                      lit ? lv_color_hex(0xe7edf0) : lv_color_hex(0x66737a), 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(button->obj,
                                  lv_color_hex(latched ? 0x101618 : 0x273237), 0);
        lv_obj_set_style_bg_grad_color(button->obj,
                                       lv_color_hex(latched ? 0x05090a : 0x0b1012), 0);
        lv_obj_set_style_bg_opa(button->obj, latched ? LV_OPA_80 : LV_OPA_90, 0);
        lv_obj_set_style_border_color(button->obj,
                                      lit || focused ? col(accent) : lv_color_hex(0x53676d), 0);
        lv_obj_set_style_outline_color(button->obj, col(accent), 0);
        lv_obj_set_style_outline_width(button->obj, focused || lit ? 2 : 1, 0);
        lv_obj_set_style_outline_opa(button->obj, focused ? LV_OPA_COVER : (lit ? LV_OPA_70 : LV_OPA_20), 0);
        lv_obj_set_style_text_color(button->text,
                                    lit || focused ? col(accent) : lv_color_hex(0xb4c0c2), 0);
        lv_obj_set_style_bg_color(button->lamp,
                                  lit ? col(accent) : lv_color_hex(0x071012), 0);
        lv_obj_set_style_border_color(button->lamp,
                                      lit ? lv_color_hex(0xdff9f4) : lv_color_hex(0x3d5055), 0);
        lv_obj_set_style_shadow_width(button->lamp, lit ? 5 : 0, 0);
        lv_obj_set_style_shadow_opa(button->lamp, lit ? LV_OPA_40 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(button->rail, col(accent), 0);
        lv_obj_set_style_bg_opa(button->rail, lit || focused ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_set_style_shadow_color(button->rail, col(accent), 0);
        lv_obj_set_style_shadow_width(button->rail, lit || focused ? 4 : 0, 0);
        lv_obj_set_style_shadow_opa(button->rail, LV_OPA_40, 0);
        lv_obj_set_style_shadow_color(button->obj, col(accent), 0);
        lv_obj_set_style_shadow_width(button->obj, focused ? 18 : (lit ? 12 : 0), 0);
        lv_obj_set_style_shadow_ofs_x(button->obj, 0, 0);
        lv_obj_set_style_shadow_ofs_y(button->obj, 0, 0);
        lv_obj_set_style_shadow_opa(button->obj,
                                    focused ? LV_OPA_70 : (lit ? LV_OPA_50 : LV_OPA_TRANSP), 0);
    }

    if(button->target_depth != target_depth) {
        lv_anim_t animation;
        button->target_depth = target_depth;
        lv_anim_delete(button, button_depth_anim_exec);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, button);
        lv_anim_set_exec_cb(&animation, button_depth_anim_exec);
        lv_anim_set_values(&animation, button->depth, target_depth);
        lv_anim_set_duration(&animation, 120);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

static void set_status(const char *text)
{
    lv_label_set_text(status_label, text);
}

static uint8_t *edited_value(void)
{
    lfo_state_t *lfo = &state.lfo[current_target];
    if(edit_mode == EDIT_LFO_RATE) return &lfo->rate;
    if(edit_mode == EDIT_LFO_DEPTH) return &lfo->depth;
    if(edit_mode == EDIT_LFO_WAVE) return &lfo->wave;
    return &state.value[current_target];
}

static void format_base_value(int target, char *buffer, size_t size)
{
    int value = state.value[target];
    if(target == T_CRUSH_BITS) {
        lv_snprintf(buffer, size, "%d bit", 4 + (value * 12) / 100);
    } else {
        lv_snprintf(buffer, size, "%d", value);
    }
}

static void refresh_ui(void);

static lv_point_precise_t rotary_polar_point(int radius, int angle)
{
    enum { AMP_CENTER = 46, TRIGO_SCALE = 32767 };
    lv_point_precise_t point;

    angle %= 360;
    point.x = AMP_CENTER + radius * lv_trigo_cos(angle) / TRIGO_SCALE;
    point.y = AMP_CENTER + radius * lv_trigo_sin(angle) / TRIGO_SCALE;
    return point;
}

static void update_rotary_indicator_geometry(rotary_knob_t *knob, int value)
{
    int angle = 135 + value * 270 / 100;

    knob->indicator_points[0] = rotary_polar_point(22, angle);
    knob->indicator_points[1] = rotary_polar_point(28, angle);
    lv_line_set_points(knob->indicator_groove, knob->indicator_points, 2);
    lv_line_set_points(knob->indicator, knob->indicator_points, 2);
}

static void rotary_anim_exec(void *object, int32_t value)
{
    rotary_knob_t *knob = (rotary_knob_t *)object;
    programmatic_rotary_update = true;
    lv_arc_set_value(knob->arc, value);
    programmatic_rotary_update = false;
    update_rotary_indicator_geometry(knob, value);
}

static void set_rotary_value(rotary_knob_t *knob, int value, bool animate)
{
    lv_anim_delete(knob, rotary_anim_exec);
    if(animate && lv_arc_get_value(knob->arc) != value) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, knob);
        lv_anim_set_exec_cb(&animation, rotary_anim_exec);
        lv_anim_set_values(&animation, lv_arc_get_value(knob->arc), value);
        lv_anim_set_duration(&animation, 130);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    } else {
        rotary_anim_exec(knob, value);
    }
}

static void style_rotary(rotary_knob_t *knob, bool selected, uint32_t accent)
{
    int tick;
    for(tick = 0; tick < 11; ++tick) {
        uint32_t tick_color = tick == 0 || tick == 10 ? C_BRASS : C_LINE;
        lv_obj_set_style_line_color(knob->ticks[tick], col(tick_color), 0);
        lv_obj_set_style_line_opa(knob->ticks[tick], selected ? LV_OPA_COVER : LV_OPA_80, 0);
    }
    lv_obj_set_style_border_color(knob->rim, col(selected ? accent : C_BRASS), 0);
    lv_obj_set_style_shadow_color(knob->rim, col(selected ? accent : 0x050504), 0);
    lv_obj_set_style_shadow_opa(knob->rim, selected ? LV_OPA_30 : LV_OPA_60, 0);
    lv_obj_set_style_bg_color(knob->body, col(selected ? 0x3b3325 : 0x292a24), 0);
    lv_obj_set_style_bg_grad_color(knob->body, col(selected ? 0x17130e : 0x10110f), 0);
    lv_obj_set_style_border_color(knob->body, col(selected ? 0x8c672d : 0x514936), 0);
    lv_obj_set_style_line_color(knob->indicator, col(selected ? accent : C_TEXT), 0);
    lv_obj_set_style_text_color(knob->value, col(selected ? C_TEXT : C_MUTED), 0);
    lv_obj_set_style_text_color(knob->caption, col(selected ? accent : C_MUTED), 0);
    if(is_metal_theme()) {
        for(tick = 0; tick < 11; ++tick) {
            lv_obj_set_style_line_color(knob->ticks[tick],
                                        tick == 0 || tick == 10
                                            ? lv_color_hex(0xb7c2c7)
                                            : lv_color_hex(0x68767d), 0);
        }
        lv_obj_set_style_bg_color(knob->rim, lv_color_hex(0x12181c), 0);
        lv_obj_set_style_border_color(knob->rim,
                                      selected ? col(accent) : lv_color_hex(0x9ba7ac), 0);
        lv_obj_set_style_shadow_color(knob->rim, lv_color_hex(0x020304), 0);
        lv_obj_set_style_shadow_opa(knob->rim, LV_OPA_80, 0);
        lv_obj_set_style_bg_color(knob->body,
                                  lv_color_hex(selected ? 0x718087 : 0x65737a), 0);
        lv_obj_set_style_bg_grad_color(knob->body,
                                       lv_color_hex(selected ? 0x252f34 : 0x1d252a), 0);
        lv_obj_set_style_border_color(knob->body, lv_color_hex(0xc0c9cd), 0);
        lv_obj_set_style_line_color(knob->indicator,
                                    selected ? col(accent) : lv_color_hex(0xf0f3f4), 0);
        lv_obj_set_style_text_color(knob->value, lv_color_hex(0xf0f3f4), 0);
        lv_obj_set_style_text_color(knob->caption,
                                    selected ? col(accent) : lv_color_hex(0xb5c0c5), 0);
    } else if(is_neon_theme()) {
        for(tick = 0; tick < 11; ++tick) {
            lv_obj_set_style_line_color(knob->ticks[tick],
                                        tick == 0 ? lv_color_hex(NEON_MAGENTA) :
                                        (tick == 10 ? lv_color_hex(NEON_LIME) : col(accent)), 0);
            lv_obj_set_style_line_opa(knob->ticks[tick], selected ? LV_OPA_COVER : LV_OPA_70, 0);
        }
        lv_obj_set_style_bg_color(knob->rim, lv_color_hex(0x070a0b), 0);
        lv_obj_set_style_border_color(knob->rim,
                                      selected ? col(accent) : lv_color_hex(0x4b5c61), 0);
        lv_obj_set_style_shadow_color(knob->rim, selected ? col(accent) : lv_color_hex(0x010203), 0);
        lv_obj_set_style_shadow_width(knob->rim, selected ? 8 : 10, 0);
        lv_obj_set_style_shadow_opa(knob->rim, selected ? LV_OPA_30 : LV_OPA_80, 0);
        lv_obj_set_style_bg_color(knob->body, lv_color_hex(selected ? 0x26343a : 0x1a2428), 0);
        lv_obj_set_style_bg_grad_color(knob->body, lv_color_hex(0x020507), 0);
        lv_obj_set_style_border_color(knob->body, selected ? col(accent) : lv_color_hex(0x40545b), 0);
        lv_obj_set_style_line_color(knob->indicator,
                                    selected ? col(accent) : lv_color_hex(0xb8df43), 0);
        lv_obj_set_style_text_color(knob->value, lv_color_hex(0xe6eeee), 0);
        lv_obj_set_style_text_color(knob->caption,
                                    selected ? col(accent) : lv_color_hex(0x9eafb2), 0);
        lv_obj_set_style_bg_color(knob->value, lv_color_hex(0x02080a), 0);
        lv_obj_set_style_bg_opa(knob->value, LV_OPA_80, 0);
        lv_obj_set_style_border_width(knob->value, 1, 0);
        lv_obj_set_style_border_color(knob->value, selected ? col(accent) : lv_color_hex(0x28464d), 0);
        lv_obj_set_style_radius(knob->value, 1, 0);
    }
}

static void style_amp_envelope_dot(lv_obj_t *dot, bool active)
{
    lv_obj_set_style_bg_color(dot, col(active ? 0x62e6cf : 0x28564e), 0);
    lv_obj_set_style_bg_opa(dot, active ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(dot, col(0x42d9c1), 0);
    lv_obj_set_style_shadow_width(dot, active ? 6 : 0, 0);
    lv_obj_set_style_shadow_spread(dot, active ? 1 : 0, 0);
    lv_obj_set_style_shadow_opa(dot, active ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    if(is_neon_theme()) {
        lv_obj_set_style_bg_color(dot, lv_color_hex(active ? NEON_CYAN : 0x163237), 0);
        lv_obj_set_style_bg_opa(dot, active ? LV_OPA_COVER : LV_OPA_30, 0);
        lv_obj_set_style_shadow_color(dot, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_shadow_width(dot, active ? 4 : 0, 0);
        lv_obj_set_style_shadow_opa(dot, active ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }
}

static int amp_envelope_y_at_x(int x)
{
    int segment;

    for(segment = 1; segment < 5; ++segment) {
        int x0 = amp_envelope_points[segment - 1].x;
        int x1 = amp_envelope_points[segment].x;
        int y0 = amp_envelope_points[segment - 1].y;
        int y1 = amp_envelope_points[segment].y;
        if(x <= x1) return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
    }
    return amp_envelope_points[4].y;
}

static void update_amp_envelope_geometry(int attack, int decay, int sustain, int release)
{
    enum { ENV_W = 420, ENV_BASELINE = 54, ENV_PEAK = 2 };
    int attack_x = 30 + attack * 70 / 100;
    int decay_x = attack_x + 35 + decay * 70 / 100;
    int release_x = ENV_W - (35 + release * 70 / 100);
    int sustain_y = ENV_BASELINE - sustain * (ENV_BASELINE - ENV_PEAK - 3) / 100;
    int i;
    int label_centers[4];

    amp_envelope_points[0] = (lv_point_precise_t){0, ENV_BASELINE};
    amp_envelope_points[1] = (lv_point_precise_t){attack_x, ENV_PEAK};
    amp_envelope_points[2] = (lv_point_precise_t){decay_x, sustain_y};
    amp_envelope_points[3] = (lv_point_precise_t){release_x, sustain_y};
    amp_envelope_points[4] = (lv_point_precise_t){ENV_W, ENV_BASELINE};

    for(i = 0; i < AMP_ENV_COLS; ++i) {
        int x = i * ENV_W / (AMP_ENV_COLS - 1);
        int y = amp_envelope_y_at_x(x);
        int row = ((y - ENV_PEAK) * (AMP_ENV_ROWS - 1) +
                   (ENV_BASELINE - ENV_PEAK) / 2) / (ENV_BASELINE - ENV_PEAK);
        row = LV_CLAMP(0, row, AMP_ENV_ROWS - 1);
        if(amp_envelope_active_rows[i] != row) {
            if(amp_envelope_active_rows[i] >= 0) {
                style_amp_envelope_dot(amp_envelope_dots[i][amp_envelope_active_rows[i]], false);
            }
            style_amp_envelope_dot(amp_envelope_dots[i][row], true);
            amp_envelope_active_rows[i] = (int8_t)row;
        }
    }

    label_centers[0] = attack_x / 2;
    label_centers[1] = (attack_x + decay_x) / 2;
    label_centers[2] = (decay_x + release_x) / 2;
    label_centers[3] = (release_x + ENV_W) / 2;
    for(i = 0; i < 4; ++i) lv_obj_set_x(amp_stage_labels[i], label_centers[i] - 10);
}

static void amp_envelope_anim_exec(void *object, int32_t progress)
{
    int i;
    (void)object;
    for(i = 0; i < 4; ++i) {
        int delta = amp_envelope_anim_target[i] - amp_envelope_anim_start[i];
        amp_envelope_values[i] = (int16_t)(amp_envelope_anim_start[i] + delta * progress / 1000);
    }
    update_amp_envelope_geometry(amp_envelope_values[0], amp_envelope_values[1],
                                 amp_envelope_values[2], amp_envelope_values[3]);
}

static void set_amp_envelope_values(bool animate)
{
    int i;
    int target[4] = {state.value[T_ATK], state.value[T_DEC], state.value[T_SUS], state.value[T_REL]};

    lv_anim_delete(amp_envelope_module, amp_envelope_anim_exec);
    if(amp_envelope_values[0] < 0) animate = false;
    if(animate) {
        bool changed = false;
        lv_anim_t animation;
        for(i = 0; i < 4; ++i) {
            amp_envelope_anim_start[i] = amp_envelope_values[i];
            amp_envelope_anim_target[i] = (int16_t)target[i];
            changed = changed || amp_envelope_anim_start[i] != amp_envelope_anim_target[i];
        }
        if(!changed) return;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, amp_envelope_module);
        lv_anim_set_exec_cb(&animation, amp_envelope_anim_exec);
        lv_anim_set_values(&animation, 0, 1000);
        lv_anim_set_duration(&animation, 130);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    } else {
        for(i = 0; i < 4; ++i) amp_envelope_values[i] = (int16_t)target[i];
        update_amp_envelope_geometry(target[0], target[1], target[2], target[3]);
    }
}

static void source_event(lv_event_t *event)
{
    int source = (int)(intptr_t)lv_event_get_user_data(event);
    state.source = (uint8_t)source;
    state.timbre = 0;
    active_preset = -1;
    mic_recording = false;
    if(source != SOURCE_MIC) mic_ready = false;
    set_status(source == SOURCE_I2S   ? "I2S input selected"
               : source == SOURCE_UDP ? "UDP wireless input selected"
                                      : "Oscillator source selected");
    refresh_ui();
    notify_state_changed();
}

static void mic_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    state.source = SOURCE_MIC;
    state.timbre = 0;
    active_preset = -1;
    if(code == LV_EVENT_PRESSED) {
        mic_recording = true;
        mic_ready = false;
        mic_started_ms = lv_tick_get();
        set_status("MIC recording - release to sample");
    } else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        uint32_t held_ms = lv_tick_get() - mic_started_ms;
        mic_recording = false;
        mic_ready = held_ms >= 1000;
        set_status(mic_ready ? "MIC sample captured and ready" : "MIC hold was too short; sample discarded");
    }
    refresh_ui();
    notify_state_changed();
    notify_mic_recording_changed(mic_recording);
}

static void page_event(lv_event_t *event)
{
    current_page = (int)(intptr_t)lv_event_get_user_data(event);
    if(current_page != PAGE_LFO) edit_mode = EDIT_BASE;
    animate_control_updates = true;
    set_status(current_page == PAGE_LFO ? "LFO editor follows the current parameter" : "Control page selected");
    refresh_ui();
}

static void rotary_event(lv_event_t *event)
{
    rotary_knob_t *knob = (rotary_knob_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    bool selection_changed;

    if(refreshing || programmatic_rotary_update) return;
    lv_anim_delete(knob, rotary_anim_exec);
    if(knob->role == ROTARY_AMP) {
        selection_changed = current_target != knob->index || edit_mode != EDIT_BASE;
        current_target = knob->index;
        edit_mode = EDIT_BASE;
    } else if(knob->role == ROTARY_FX) {
        int target = fx_target_base[current_fx] + knob->index;
        selection_changed = current_target != target || edit_mode != EDIT_BASE;
        current_target = target;
        edit_mode = EDIT_BASE;
    } else {
        int mode = EDIT_LFO_RATE + knob->index;
        selection_changed = edit_mode != mode;
        edit_mode = mode;
    }
    if(code == LV_EVENT_VALUE_CHANGED) {
        int value = lv_arc_get_value(knob->arc);
        if(knob->role == ROTARY_LFO) *edited_value() = (uint8_t)value;
        else state.value[current_target] = (uint8_t)value;
        animate_control_updates = false;
    } else if(selection_changed) {
        animate_control_updates = true;
    }
    set_status(knob->role == ROTARY_AMP ? "Amplifier parameter selected" :
               (knob->role == ROTARY_FX ? "Effect parameter selected" : "LFO parameter selected"));
    refresh_ui();
    if(code == LV_EVENT_VALUE_CHANGED) notify_state_changed();
}

static void effect_event(lv_event_t *event)
{
    int effect = (int)(intptr_t)lv_event_get_user_data(event);
    current_fx = effect;
    state.effect_on[effect] = !state.effect_on[effect];
    current_target = fx_target_base[effect];
    edit_mode = EDIT_BASE;
    animate_control_updates = true;
    set_status(state.effect_on[effect] ? "Effect enabled; parameters ready" : "Effect bypassed; parameters remain editable");
    refresh_ui();
    notify_state_changed();
}

static void lfo_power_event(lv_event_t *event)
{
    (void)event;
    state.lfo[current_target].enabled = !state.lfo[current_target].enabled;
    if(state.lfo[current_target].enabled && edit_mode == EDIT_BASE) edit_mode = EDIT_LFO_RATE;
    animate_control_updates = true;
    set_status(state.lfo[current_target].enabled ? "LFO enabled for current target" : "LFO bypassed for current target");
    refresh_ui();
    notify_state_changed();
}

static void lfo_wave_event(lv_event_t *event)
{
    static const uint8_t wave_values[4] = {0, 40, 70, 100};
    int wave = (int)(intptr_t)lv_event_get_user_data(event);
    state.lfo[current_target].wave = wave_values[wave];
    edit_mode = EDIT_LFO_WAVE;
    animate_control_updates = true;
    set_status("LFO waveform selected");
    refresh_ui();
    notify_state_changed();
}

static void update_fader_cap_geometry(int value)
{
    enum { FADER_X = 24, FADER_WIDTH = 596, CAP_WIDTH = 40 };
    fader_visual_value = (int16_t)value;
    lv_obj_set_x(fader_cap, FADER_X + value * (FADER_WIDTH - CAP_WIDTH) / 100);
}

static void fader_anim_exec(void *object, int32_t value)
{
    (void)object;
    update_fader_cap_geometry(value);
}

static void set_fader_value(int value, bool animate)
{
    lv_anim_delete(fader_cap, fader_anim_exec);
    lv_slider_set_value(common_slider, value, LV_ANIM_OFF);
    if(animate && fader_visual_value >= 0 && fader_visual_value != value) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, fader_cap);
        lv_anim_set_exec_cb(&animation, fader_anim_exec);
        lv_anim_set_values(&animation, fader_visual_value, value);
        lv_anim_set_duration(&animation, 140);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    } else {
        update_fader_cap_geometry(value);
    }
}

static void slider_event(lv_event_t *event)
{
    int value = lv_slider_get_value(lv_event_get_target_obj(event));
    if(refreshing) return;
    lv_anim_delete(fader_cap, fader_anim_exec);
    update_fader_cap_geometry(value);
    *edited_value() = (uint8_t)value;
    animate_control_updates = false;
    refresh_ui();
    notify_state_changed();
}

static void mode_event(lv_event_t *event)
{
    state.mode = (uint8_t)(intptr_t)lv_event_get_user_data(event);
    set_status(state.mode == 0 ? "XY pitch quantized to semitones" : "XY pitch is continuous");
    refresh_ui();
    notify_state_changed();
}

static void join_dirty_area(lv_area_t *destination, const lv_area_t *area)
{
    destination->x1 = LV_MIN(destination->x1, area->x1);
    destination->y1 = LV_MIN(destination->y1, area->y1);
    destination->x2 = LV_MAX(destination->x2, area->x2);
    destination->y2 = LV_MAX(destination->y2, area->y2);
}

static void xy_event(lv_event_t *event)
{
    lv_point_t point;
    lv_area_t area;
    int x;
    int y;
    int pitch_tenths;
    char text[48];
    if(lv_event_get_code(event) == LV_EVENT_RELEASED || lv_event_get_code(event) == LV_EVENT_PRESS_LOST) {
        if(!lv_obj_has_flag(xy_marker, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_get_coords(xy_marker, &xy_marker_dirty_area);
            lv_area_increase(&xy_marker_dirty_area, 12, 12);
            xy_marker_visual_dirty = true;
            lv_obj_add_flag(xy_marker, LV_OBJ_FLAG_HIDDEN);
        }
        notify_xy_note_changed(0.0f, false);
        return;
    }
    if(lv_event_get_code(event) != LV_EVENT_PRESSED && lv_event_get_code(event) != LV_EVENT_PRESSING) return;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_obj_get_content_coords(lv_event_get_target_obj(event), &area);
    if(!lv_obj_has_flag(xy_marker, LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t old_area;
        lv_obj_get_coords(xy_marker, &old_area);
        lv_area_increase(&old_area, 12, 12);
        if(xy_marker_visual_dirty) join_dirty_area(&xy_marker_dirty_area, &old_area);
        else xy_marker_dirty_area = old_area;
    }
    x = LV_CLAMP(10, point.x - area.x1, lv_area_get_width(&area) - 10);
    y = LV_CLAMP(10, point.y - area.y1, lv_area_get_height(&area) - 10);
    lv_obj_remove_flag(xy_marker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(xy_marker, x - 10, y - 10);
    {
        lv_area_t new_area = {
            .x1 = area.x1 + x - 22,
            .y1 = area.y1 + y - 22,
            .x2 = area.x1 + x + 22,
            .y2 = area.y1 + y + 22,
        };
        if(xy_marker_visual_dirty) join_dirty_area(&xy_marker_dirty_area, &new_area);
        else xy_marker_dirty_area = new_area;
        xy_marker_visual_dirty = true;
    }
    pitch_tenths = 480 + (x * 480) / lv_area_get_width(&area);
    if(state.mode == 0) {
        int midi_note = (pitch_tenths + 5) / 10;
        lv_snprintf(text, sizeof(text), "MIDI %02d  /  SEMITONE", midi_note);
    } else {
        lv_snprintf(text, sizeof(text), "MIDI %02d.%d  /  CONTINUOUS", pitch_tenths / 10, pitch_tenths % 10);
    }
    lv_label_set_text(xy_readout, text);
    xy_readout_visual_dirty = true;
    notify_xy_note_changed(state.mode == 0 ? (float)((pitch_tenths + 5) / 10) : (float)pitch_tenths / 10.0f, true);
}

static uint32_t shade_color(uint32_t color, int progress, int darken_percent)
{
    int scale = 1000 - progress * darken_percent / 100;
    int red = ((color >> 16) & 0xff) * scale / 1000;
    int green = ((color >> 8) & 0xff) * scale / 1000;
    int blue = (color & 0xff) * scale / 1000;
    return (uint32_t)((red << 16) | (green << 8) | blue);
}

static void apply_key_press(piano_key_t *key, int32_t progress)
{
    int inset = progress * (key->black ? 1 : 2) / 1000;
    int sink = progress * (key->black ? 3 : 4) / 1000;
    int shorten = progress * (key->black ? 5 : 7) / 1000;
    int base_shadow = key->black ? 8 : 3;
    int base_shadow_y = key->black ? 5 : 3;
    uint32_t base_color = key->black ? 0x121310 : 0xe8ddbd;

    key->press_progress = (int16_t)progress;
    lv_obj_set_pos(key->obj, key->x + inset, key->y + sink);
    lv_obj_set_size(key->obj, key->width - inset * 2, key->height - shorten);
    lv_obj_set_style_bg_color(key->obj, col(shade_color(base_color, progress, key->black ? 28 : 16)), 0);
    lv_obj_set_style_border_color(key->obj,
                                  col(shade_color(key->black ? 0x414036 : 0x766e58,
                                                  progress, key->black ? 20 : 14)), 0);
    lv_obj_set_style_shadow_width(key->obj, base_shadow - progress * (base_shadow - 1) / 1000, 0);
    lv_obj_set_style_shadow_ofs_y(key->obj, base_shadow_y - progress * (base_shadow_y - 1) / 1000, 0);
    lv_obj_set_style_shadow_opa(key->obj, LV_OPA_60 - progress * 34 / 1000, 0);
}

static void key_press_anim_exec(void *object, int32_t progress)
{
    apply_key_press((piano_key_t *)object, progress);
}

static void animate_key_press(piano_key_t *key, bool down)
{
    int target = down ? 1000 : 0;
#if defined(LVGL_SYNTH_EMBEDDED)
    lv_anim_delete(key, key_press_anim_exec);
    if(key->press_progress == target) return;
    key->visual_dirty = true;
    apply_key_press(key, target);
    return;
#else
    lv_anim_t animation;

    lv_anim_delete(key, key_press_anim_exec);
    if(key->press_progress == target) return;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, key);
    lv_anim_set_exec_cb(&animation, key_press_anim_exec);
    lv_anim_set_values(&animation, key->press_progress, target);
    lv_anim_set_duration(&animation, down ? 70 : 105);
    lv_anim_set_path_cb(&animation, down ? lv_anim_path_ease_out : lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
#endif
}

static void key_event(lv_event_t *event)
{
    piano_key_t *key = (piano_key_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);

    if(code == LV_EVENT_PRESSED) {
        animate_key_press(key, true);
        float output_note = (float)key->midi_note;
#ifdef LVGL_SYNTH_EMBEDDED
        output_note += 12.0f;
#endif
        notify_note_changed(output_note, true);
    } else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        animate_key_press(key, false);
        float output_note = (float)key->midi_note;
#ifdef LVGL_SYNTH_EMBEDDED
        output_note += 12.0f;
#endif
        notify_note_changed(output_note, false);
    }
}

#if defined(LVGL_SYNTH_EMBEDDED)
static float key_output_note(const piano_key_t *key)
{
    return (float)key->midi_note + 12.0f;
}

static piano_key_t *keyboard_key_at(const lv_point_t *point)
{
    lv_area_t keyboard_area;
    int i;

    lv_obj_get_content_coords(keyboard_panel, &keyboard_area);
    for(i = 0; i < 10; ++i) {
        piano_key_t *key = &black_keys[i];
        int x = point->x - keyboard_area.x1;
        int y = point->y - keyboard_area.y1;
        if(x >= key->x && x < key->x + key->width &&
           y >= key->y && y < key->y + key->height) return key;
    }
    for(i = 0; i < 15; ++i) {
        piano_key_t *key = &white_keys[i];
        int x = point->x - keyboard_area.x1;
        int y = point->y - keyboard_area.y1;
        if(x >= key->x && x < key->x + key->width &&
           y >= key->y && y < key->y + key->height) return key;
    }
    return NULL;
}

static keyboard_gesture_t *keyboard_gesture_for(lv_indev_t *indev, bool allocate)
{
    keyboard_gesture_t *free_slot = NULL;
    int i;

    for(i = 0; i < 5; ++i) {
        if(keyboard_gestures[i].indev == indev) return &keyboard_gestures[i];
        if(free_slot == NULL && keyboard_gestures[i].indev == NULL) free_slot = &keyboard_gestures[i];
    }
    if(allocate && free_slot != NULL) {
        free_slot->indev = indev;
        free_slot->midi_note = -1;
        return free_slot;
    }
    return NULL;
}

static piano_key_t *keyboard_key_for_note(int16_t midi_note)
{
    int i;
    for(i = 0; i < 10; ++i) {
        if(black_keys[i].midi_note == midi_note) return &black_keys[i];
    }
    for(i = 0; i < 15; ++i) {
        if(white_keys[i].midi_note == midi_note) return &white_keys[i];
    }
    return NULL;
}

static void keyboard_release_gesture(keyboard_gesture_t *gesture)
{
    piano_key_t *key;
    if(gesture == NULL) return;
    key = keyboard_key_for_note(gesture->midi_note);
    if(key != NULL) {
        animate_key_press(key, false);
        notify_note_changed(key_output_note(key), false);
    }
    gesture->indev = NULL;
    gesture->midi_note = -1;
}

static void keyboard_swipe_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();
    keyboard_gesture_t *gesture;
    piano_key_t *key;
    lv_point_t point;

    if(indev == NULL) return;
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        keyboard_release_gesture(keyboard_gesture_for(indev, false));
        return;
    }
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;

    gesture = keyboard_gesture_for(indev, true);
    if(gesture == NULL) return;
    lv_indev_get_point(indev, &point);
    key = keyboard_key_at(&point);
    if(key == NULL) {
        keyboard_release_gesture(gesture);
        return;
    }
    if(gesture->midi_note == key->midi_note) return;

    if(gesture->midi_note >= 0) {
        piano_key_t *old_key = keyboard_key_for_note(gesture->midi_note);
        if(old_key != NULL) {
            animate_key_press(old_key, false);
            notify_note_changed(key_output_note(old_key), false);
        }
    }
    gesture->midi_note = key->midi_note;
    animate_key_press(key, true);
    notify_note_changed(key_output_note(key), true);
}
#endif

static void default_patch(synth_state_t *patch)
{
    static const uint8_t defaults[TARGET_COUNT] = {
        45, 0, 6, 72, 12,
        35, 40, 30,
        30, 40, 30,
        40, 55, 0,
        100, 100, 0
    };
    int i;
    memset(patch, 0, sizeof(*patch));
    memcpy(patch->value, defaults, sizeof(defaults));
    patch->source = SOURCE_SINE;
    patch->mode = 1;
    patch->timbre = 0;
    patch->effect_on[FX_DELAY] = false;
    patch->effect_on[FX_CHORUS] = false;
    for(i = 0; i < TARGET_COUNT; ++i) {
        patch->lfo[i].rate = 35;
        patch->lfo[i].depth = 45;
        patch->lfo[i].wave = 20;
    }
}

static void apply_preset(int preset)
{
    typedef struct {
        uint8_t source;
        uint8_t value[TARGET_COUNT];
        bool effect_on[FX_COUNT];
    } preset_t;
    static const preset_t presets[10] = {
        {SOURCE_SAW,      {62,0,24,8,16,  16,18,8,  18,10,0, 28,52,0, 98,98,0}, {true,false,false,false}},
        {SOURCE_TRIANGLE, {66,0,46,6,24,  18,16,7,  16,8,0,  24,50,0, 98,98,0}, {true,false,false,false}},
        {SOURCE_SQUARE,   {57,1,3,96,12,  16,14,5,  12,18,14,24,50,0, 98,98,0}, {false,true,false,false}},
        {SOURCE_SINE,     {54,3,10,88,14, 10,12,0,  10,8,0,  20,48,0, 98,98,0}, {false,false,false,false}},
        {SOURCE_TRIANGLE, {52,36,24,82,64, 30,28,18, 12,30,24,18,48,0, 98,98,0}, {true,true,false,false}},
        {SOURCE_SQUARE,   {60,0,18,0,12,  14,18,10, 16,8,0,  26,54,0, 98,98,0}, {true,false,false,false}},
        {SOURCE_SINE,     {60,0,48,0,42,  34,28,22, 16,10,0,  20,50,0, 98,98,0}, {true,false,false,false}},
        {SOURCE_SAW,      {65,4,14,78,18, 12,12,0,  12,8,0,  22,58,7, 98,98,0}, {false,false,true,false}},
        {SOURCE_SQUARE,   {68,1,20,58,10,  8,10,0,  8,8,0,  18,44,5, 98,98,0}, {false,false,true,false}},
        {SOURCE_SAW,      {60,1,22,58,30, 24,42,24, 24,38,24,52,62,24,54,52,16}, {true,true,true,true}}
    };
    static const char *const names[11] = {"GTR", "PNO", "ORG", "REC", "PAD", "PLK", "BEL", "BRS", "BAS", "SYN", "RND"};
    int i;
    int seed = (preset + 3) * 17;
    default_patch(&state);
    if(preset == 10) {
        for(i = 0; i < TARGET_COUNT; ++i) state.value[i] = (uint8_t)((seed + i * 37) % 101);
        state.source = (uint8_t)(seed % 4);
        state.timbre = (uint8_t)(seed % 11);
        for(i = 0; i < FX_COUNT; ++i) state.effect_on[i] = ((seed >> i) & 1) != 0;
    } else {
        state.source = presets[preset].source;
        state.timbre = (uint8_t)(preset + 1);
        memcpy(state.value, presets[preset].value, sizeof(state.value));
        memcpy(state.effect_on, presets[preset].effect_on, sizeof(state.effect_on));
    }
    mic_ready = false;
    memories[current_memory] = state;
    active_preset = preset;
    current_target = T_VOL;
    edit_mode = EDIT_BASE;
    {
        char message[48];
        lv_snprintf(message, sizeof(message), "Preset %s loaded", names[preset]);
        set_status(message);
    }
}

static void bank_event(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if(index < 11) {
        memories[current_memory] = state;
        apply_preset(index);
    } else {
        int slot = index - 11;
        memories[current_memory] = state;
        state = memories[slot];
        current_memory = slot;
        active_preset = -1;
        current_target = T_VOL;
        edit_mode = EDIT_BASE;
        mic_recording = false;
        mic_ready = state.source == SOURCE_MIC;
        {
            char message[48];
            lv_snprintf(message, sizeof(message), "Memory M%d loaded; prior memory stored", slot + 1);
            set_status(message);
        }
    }
    animate_control_updates = true;
    refresh_ui();
    notify_state_changed();
}

static void refresh_ui(void)
{
    int i;
    char text[64];
    bool animate_controls = animate_control_updates;
    animate_control_updates = false;
    refreshing = true;

    for(i = 0; i < SOURCE_COUNT; ++i) {
        bool selected = state.source == i;
        style_button(&source_buttons[i], selected, selected, selected, i == SOURCE_MIC ? C_RED : C_CYAN);
    }
    if(mic_recording) {
        lv_label_set_text(source_buttons[SOURCE_MIC].text, "MIC  REC");
    } else if(mic_ready && state.source == SOURCE_MIC) {
        lv_label_set_text(source_buttons[SOURCE_MIC].text, "MIC  READY");
    } else {
        lv_label_set_text(source_buttons[SOURCE_MIC].text, "MIC");
    }
    if(mic_recording) lv_label_set_text(source_readout, "RECORDING");
    else if(mic_ready && state.source == SOURCE_MIC) lv_label_set_text(source_readout, "SAMPLED / READY");
    else if(state.source == SOURCE_I2S) lv_label_set_text(source_readout, "DIGITAL INPUT");
    else if(state.source == SOURCE_UDP) lv_label_set_text(source_readout, "WIRELESS INPUT");
    else lv_label_set_text(source_readout, "SOURCE ACTIVE");

    for(i = 0; i < PAGE_COUNT; ++i) {
        bool selected = current_page == i;
        style_button(&page_buttons[i], selected, selected, selected, i == PAGE_LFO ? C_CYAN : C_AMBER);
        if(current_page == i) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    for(i = 0; i < 5; ++i) {
        char value[12];
        bool selected = current_target == i && edit_mode == EDIT_BASE;
        set_rotary_value(&amp_knobs[i], state.value[i], animate_controls);
        format_base_value(i, value, sizeof(value));
        lv_label_set_text(amp_knobs[i].value, value);
        style_rotary(&amp_knobs[i], selected, C_AMBER);
    }
    set_amp_envelope_values(animate_controls);

    for(i = 0; i < FX_COUNT; ++i) {
        static const char *const names[FX_COUNT] = {"DELAY", "CHORUS", "DRIVE", "CRUSH"};
        lv_snprintf(text, sizeof(text), "%s  %s", names[i], state.effect_on[i] ? "ON" : "OFF");
        lv_label_set_text(effect_buttons[i].text, text);
        style_button(&effect_buttons[i], state.effect_on[i], current_fx == i, state.effect_on[i], C_RED);
    }
    for(i = 0; i < 3; ++i) {
        int target = fx_target_base[current_fx] + i;
        char value[16];
        bool selected = current_target == target && edit_mode == EDIT_BASE;
        set_rotary_value(&fx_knobs[i], state.value[target], animate_controls);
        format_base_value(target, value, sizeof(value));
        lv_label_set_text(fx_knobs[i].value, value);
        lv_label_set_text(fx_knobs[i].caption, short_target_names[target]);
        style_rotary(&fx_knobs[i], selected, C_AMBER);
    }

    lv_snprintf(text, sizeof(text), "TARGET  %s", target_names[current_target]);
    lv_label_set_text(lfo_target_label, text);
    lv_snprintf(text, sizeof(text), "RAT %02d   DEP %02d   WAV %s", state.lfo[current_target].rate,
                state.lfo[current_target].depth, lfo_wave_name(state.lfo[current_target].wave));
    lv_label_set_text(lfo_state_label, text);
    lv_label_set_text(lfo_power_button.text, state.lfo[current_target].enabled ? "LFO ON" : "LFO OFF");
    style_button(&lfo_power_button, state.lfo[current_target].enabled, state.lfo[current_target].enabled,
                 state.lfo[current_target].enabled, C_CYAN);
    for(i = 0; i < 2; ++i) {
        int value = i == 0 ? state.lfo[current_target].rate : state.lfo[current_target].depth;
        bool selected = edit_mode == EDIT_LFO_RATE + i;
        set_rotary_value(&lfo_knobs[i], value, animate_controls);
        lv_snprintf(text, sizeof(text), "%d", value);
        lv_label_set_text(lfo_knobs[i].value, text);
        style_rotary(&lfo_knobs[i], selected, C_CYAN);
    }
    {
        uint8_t wave = state.lfo[current_target].wave;
        int selected_wave = lfo_wave_index(wave);
        for(i = 0; i < 4; ++i) {
            bool selected = i == selected_wave;
            style_button(&lfo_wave_buttons[i], selected, selected, selected, C_CYAN);
        }
    }

    for(i = 0; i < 16; ++i) {
        bool selected = i < 11 ? active_preset == i : (active_preset < 0 && current_memory == i - 11);
        style_button(&bank_buttons[i], selected, selected, selected, i >= 11 ? C_CYAN : C_AMBER);
    }
    for(i = 0; i < 2; ++i) {
        bool selected = state.mode == i;
        style_button(&mode_buttons[i], selected, selected, selected, C_LIME);
    }

    if(edit_mode == EDIT_BASE) {
        char value[16];
        format_base_value(current_target, value, sizeof(value));
        lv_snprintf(text, sizeof(text), "BASE  /  %s", target_names[current_target]);
        lv_label_set_text(target_label, text);
        lv_label_set_text(target_value, value);
        lv_slider_set_range(common_slider, 0, 100);
    } else {
        const char *field = edit_mode == EDIT_LFO_RATE ? "RAT" : (edit_mode == EDIT_LFO_DEPTH ? "DEP" : "WAV");
        lv_snprintf(text, sizeof(text), "LFO  /  %s  /  %s", target_names[current_target], field);
        lv_label_set_text(target_label, text);
        if(edit_mode == EDIT_LFO_WAVE) {
            lv_label_set_text(target_value, lfo_wave_name(state.lfo[current_target].wave));
            lv_slider_set_range(common_slider, 0, 100);
        } else {
            lv_snprintf(text, sizeof(text), "%d", *edited_value());
            lv_label_set_text(target_value, text);
            lv_slider_set_range(common_slider, 0, 100);
        }
    }
    if(edit_mode == EDIT_LFO_WAVE) {
        lv_obj_add_flag(common_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fader_cap, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(common_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(fader_cap, LV_OBJ_FLAG_HIDDEN);
        set_fader_value(*edited_value(), animate_controls);
    }
    refreshing = false;
}

static void cycle_theme_async(void *unused)
{
    static const char *const names[] = {"WOOD THEME", "METAL THEME", "NEON THEME"};
    lv_obj_t *screen = lv_screen_active();
    (void)unused;
    current_theme = (lvgl_synth_theme_t)(((int)current_theme + 1) % 3);
    lv_obj_clean(screen);
    lvgl_synth_ui_create();
    set_status(names[(int)current_theme]);
}

static void theme_event(lv_event_t *event)
{
    (void)event;
    lv_async_call(cycle_theme_async, NULL);
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *header = make_rect(screen, 0, 0, 1280, 42, 0x171713);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, col(C_BRASS), 0);
    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(header, lv_color_hex(0x171e22), 0);
        lv_obj_set_style_bg_grad_color(header, lv_color_hex(0x303a3f), 0);
        lv_obj_set_style_bg_grad_dir(header, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(header, lv_color_hex(0x839097), 0);
        lv_obj_set_style_shadow_width(header, 6, 0);
        lv_obj_set_style_shadow_ofs_y(header, 3, 0);
        lv_obj_set_style_shadow_color(header, lv_color_hex(0x020304), 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(header, lv_color_hex(0x080d0f), 0);
        lv_obj_set_style_bg_grad_color(header, lv_color_hex(0x1b2528), 0);
        lv_obj_set_style_bg_grad_dir(header, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(header, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_shadow_width(header, 20, 0);
        lv_obj_set_style_shadow_ofs_y(header, 0, 0);
        lv_obj_set_style_shadow_color(header, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_shadow_opa(header, LV_OPA_60, 0);
        make_neon_rect(header, 330, 8, 72, 2, NEON_CYAN, LV_OPA_COVER, 10);
        make_neon_rect(header, 406, 8, 34, 2, NEON_MAGENTA, LV_OPA_COVER, 9);
        make_neon_rect(header, 444, 8, 52, 2, NEON_LIME, LV_OPA_COVER, 9);
        make_neon_rect(header, 1087, 32, 38, 2, NEON_AMBER, LV_OPA_COVER, 8);
        make_neon_rect(header, 1129, 32, 76, 2, NEON_RED, LV_OPA_COVER, 9);
    }
    make_rect(header, 0, 0, 9, 42, C_RED);
    {
        lv_obj_t *brand = make_label(header, "TAB5", &lv_font_montserrat_20, C_TEXT);
        lv_obj_set_pos(brand, 22, 8);
        lv_obj_set_width(brand, 62);
        lv_obj_add_flag(brand, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(brand, theme_event, LV_EVENT_RELEASED, NULL);
        lv_obj_t *product = make_label(header, "PERFORMANCE SYNTHESIZER", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(product, 104, 15);
        status_label = make_label(header, "Simulator ready", &lv_font_montserrat_12, C_AMBER);
        lv_obj_set_pos(status_label, 525, 14);
        lv_obj_set_width(status_label, 545);
        lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_t *online = make_label(header, "SIM  ONLINE", &lv_font_montserrat_12, C_LIME);
        lv_obj_set_pos(online, 1112, 14);
    }
}

static void add_theme_hitbox(lv_obj_t *screen, int x, int y, int w, int h)
{
    lv_obj_t *hitbox = lv_obj_create(screen);
    lv_obj_set_pos(hitbox, x, y);
    lv_obj_set_size(hitbox, w, h);
    lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hitbox, 0, 0);
    lv_obj_set_style_pad_all(hitbox, 0, 0);
    lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(hitbox);
    lv_obj_add_event_cb(hitbox, theme_event, LV_EVENT_PRESSED, NULL);
}

static void create_theme_hitboxes(lv_obj_t *screen)
{
    add_theme_hitbox(screen, 0, 0, 300, 42);
    add_theme_hitbox(screen, 14, 48, 150, 74);
}

static void create_sources(lv_obj_t *screen)
{
    static const char *const names[SOURCE_COUNT] = {"SINE", "SAW", "SQUARE", "TRIANGLE", "MIC", "I2S", "UDP"};
    lv_obj_t *panel = make_panel(screen, 14, 48, 1252, 74);
    int i;
    add_screws(panel, 1252, 74);
    {
        lv_obj_t *title = make_label(panel, "01  SOURCE", &lv_font_montserrat_14, C_TEXT);
        lv_obj_set_pos(title, 18, 26);
    }
    for(i = 0; i < SOURCE_COUNT; ++i) {
        source_buttons[i] = make_button(panel, names[i], 155 + i * 118, 14, 108, 46,
                                        i == SOURCE_MIC ? NULL : source_event, i);
        if(i == SOURCE_MIC) {
            lv_obj_add_event_cb(source_buttons[i].obj, mic_event, LV_EVENT_PRESSED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(source_buttons[i].obj, mic_event, LV_EVENT_RELEASED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(source_buttons[i].obj, mic_event, LV_EVENT_PRESS_LOST, (void *)(intptr_t)i);
        }
    }
    source_readout = make_label(panel, "SOURCE ACTIVE", &lv_font_montserrat_12, C_AMBER);
    lv_obj_set_pos(source_readout, 1010, 27);
    lv_obj_set_width(source_readout, 228);
    lv_obj_set_style_text_align(source_readout, LV_TEXT_ALIGN_CENTER, 0);
}

static lv_obj_t *create_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_pos(page, 12, 54);
    lv_obj_set_size(page, 622, 252);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    no_scroll(page);
    return page;
}

static void create_rotary_knob(lv_obj_t *parent, rotary_knob_t *knob, int x, int y,
                               uint8_t role, uint8_t index, const char *caption)
{
    lv_obj_t *highlight;
    int tick;

    memset(knob, 0, sizeof(*knob));
    knob->role = role;
    knob->index = index;
    knob->arc = lv_arc_create(parent);
    lv_obj_set_pos(knob->arc, x, y);
    lv_obj_set_size(knob->arc, 92, 92);
    lv_arc_set_rotation(knob->arc, 135);
    lv_arc_set_bg_angles(knob->arc, 0, 270);
    lv_arc_set_range(knob->arc, 0, 100);
    lv_obj_set_style_arc_opa(knob->arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(knob->arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(knob->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(knob->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_outline_opa(knob->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(knob->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(knob->arc, 0, LV_PART_KNOB);
    for(tick = 0; tick < 11; ++tick) {
        int angle = 135 + tick * 27;
        int inner_radius = tick == 0 || tick == 10 ? 35 : 38;
        lv_obj_t *mark = lv_line_create(knob->arc);
        knob->tick_points[tick][0] = rotary_polar_point(inner_radius, angle);
        knob->tick_points[tick][1] = rotary_polar_point(44, angle);
        lv_line_set_points(mark, knob->tick_points[tick], 2);
        lv_obj_set_style_line_width(mark, tick == 0 || tick == 10 ? 3 : 2, 0);
        lv_obj_set_style_line_color(mark, col(tick == 0 || tick == 10 ? C_BRASS : C_LINE), 0);
        lv_obj_set_style_line_opa(mark, LV_OPA_80, 0);
        lv_obj_set_style_line_rounded(mark, true, 0);
        lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        knob->ticks[tick] = mark;
    }
    knob->rim = lv_obj_create(knob->arc);
    lv_obj_set_size(knob->rim, 66, 66);
    lv_obj_center(knob->rim);
    lv_obj_set_style_radius(knob->rim, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(knob->rim, col(0x11110f), 0);
    lv_obj_set_style_bg_opa(knob->rim, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(knob->rim, 3, 0);
    lv_obj_set_style_border_color(knob->rim, col(C_BRASS), 0);
    lv_obj_set_style_shadow_width(knob->rim, 8, 0);
    lv_obj_set_style_shadow_ofs_y(knob->rim, 3, 0);
    lv_obj_set_style_shadow_color(knob->rim, col(0x050504), 0);
    lv_obj_set_style_shadow_opa(knob->rim, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(knob->rim, 0, 0);
    lv_obj_remove_flag(knob->rim, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(knob->rim);
    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(knob->rim, lv_color_hex(0x11171b), 0);
        lv_obj_set_style_border_width(knob->rim, 3, 0);
        lv_obj_set_style_border_color(knob->rim, lv_color_hex(0xa5b0b5), 0);
        lv_obj_set_style_outline_width(knob->rim, 1, 0);
        lv_obj_set_style_outline_color(knob->rim, lv_color_hex(0x040608), 0);
        lv_obj_set_style_outline_pad(knob->rim, 1, 0);
        lv_obj_set_style_shadow_width(knob->rim, 10, 0);
        lv_obj_set_style_shadow_ofs_y(knob->rim, 5, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(knob->rim, lv_color_hex(0x06090a), 0);
        lv_obj_set_style_border_width(knob->rim, 3, 0);
        lv_obj_set_style_border_color(knob->rim, lv_color_hex(0x53666b), 0);
        lv_obj_set_style_outline_width(knob->rim, 2, 0);
        lv_obj_set_style_outline_color(knob->rim, lv_color_hex(NEON_MAGENTA), 0);
        lv_obj_set_style_outline_opa(knob->rim, LV_OPA_30, 0);
        lv_obj_set_style_outline_pad(knob->rim, 1, 0);
        lv_obj_set_style_shadow_width(knob->rim, 11, 0);
        lv_obj_set_style_shadow_ofs_y(knob->rim, 6, 0);
    }
    knob->body = lv_obj_create(knob->rim);
    lv_obj_set_size(knob->body, 56, 56);
    lv_obj_center(knob->body);
    lv_obj_set_style_radius(knob->body, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(knob->body, col(0x292a24), 0);
    lv_obj_set_style_bg_grad_color(knob->body, col(0x10110f), 0);
    lv_obj_set_style_bg_grad_dir(knob->body, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(knob->body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(knob->body, 2, 0);
    lv_obj_set_style_border_color(knob->body, col(0x514936), 0);
    lv_obj_set_style_pad_all(knob->body, 0, 0);
    lv_obj_remove_flag(knob->body, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(knob->body);
    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(knob->body, lv_color_hex(0x69777e), 0);
        lv_obj_set_style_bg_grad_color(knob->body, lv_color_hex(0x20292e), 0);
        lv_obj_set_style_border_width(knob->body, 2, 0);
        lv_obj_set_style_border_color(knob->body, lv_color_hex(0xc1c9cd), 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(knob->body, lv_color_hex(0x263236), 0);
        lv_obj_set_style_bg_grad_color(knob->body, lv_color_hex(0x080c0d), 0);
        lv_obj_set_style_border_width(knob->body, 3, 0);
        lv_obj_set_style_border_color(knob->body, lv_color_hex(0x40575d), 0);
    }
    highlight = make_rect(knob->body, 11, 7, 27, 5, C_TEXT);
    lv_obj_set_style_radius(highlight, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_20, 0);
    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(highlight, lv_color_hex(0xe4eaec), 0);
        lv_obj_set_style_bg_opa(highlight, LV_OPA_30, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_pos(highlight, 12, 8);
        lv_obj_set_size(highlight, 25, 3);
        lv_obj_set_style_bg_color(highlight, lv_color_hex(0x607075), 0);
        lv_obj_set_style_bg_opa(highlight, LV_OPA_30, 0);
    }
    knob->indicator_groove = lv_line_create(knob->arc);
    lv_obj_set_style_line_width(knob->indicator_groove, 5, 0);
    lv_obj_set_style_line_color(knob->indicator_groove, col(0x080806), 0);
    lv_obj_set_style_line_rounded(knob->indicator_groove, true, 0);
    lv_obj_remove_flag(knob->indicator_groove, LV_OBJ_FLAG_CLICKABLE);
    knob->indicator = lv_line_create(knob->arc);
    lv_obj_set_style_line_width(knob->indicator, 2, 0);
    lv_obj_set_style_line_color(knob->indicator, col(C_TEXT), 0);
    lv_obj_set_style_line_rounded(knob->indicator, true, 0);
    lv_obj_remove_flag(knob->indicator, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(knob->arc, rotary_event, LV_EVENT_VALUE_CHANGED, knob);
    lv_obj_add_event_cb(knob->arc, rotary_event, LV_EVENT_PRESSED, knob);
    lv_obj_add_event_cb(knob->arc, rotary_event, LV_EVENT_CLICKED, knob);
    update_rotary_indicator_geometry(knob, 0);
    knob->value = make_label(parent, "0", &lv_font_montserrat_14, C_TEXT);
    lv_obj_set_pos(knob->value, x + 20, y + 37);
    lv_obj_set_width(knob->value, 52);
    lv_obj_set_style_text_align(knob->value, LV_TEXT_ALIGN_CENTER, 0);
    if(is_neon_theme()) {
        lv_obj_set_style_bg_color(knob->value, lv_color_hex(0x02080a), 0);
        lv_obj_set_style_bg_opa(knob->value, LV_OPA_80, 0);
        lv_obj_set_style_border_width(knob->value, 1, 0);
        lv_obj_set_style_border_color(knob->value, lv_color_hex(0x28464d), 0);
        lv_obj_set_style_radius(knob->value, 1, 0);
    }
    knob->caption = make_label(parent, caption, &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(knob->caption, x, y + 104);
    lv_obj_set_width(knob->caption, 92);
    lv_obj_set_style_text_align(knob->caption, LV_TEXT_ALIGN_CENTER, 0);
}

static void create_amp_page(lv_obj_t *parent)
{
    int i;
    pages[PAGE_AMP] = create_page(parent);
    for(i = 0; i < 5; ++i) {
        create_rotary_knob(pages[PAGE_AMP], &amp_knobs[i], 16 + i * 121, 20,
                           ROTARY_AMP, (uint8_t)i, short_target_names[i]);
    }
    {
        lv_obj_t *hint = make_label(pages[PAGE_AMP], "AMPLIFIER ENVELOPE", &lv_font_montserrat_12, C_LIME);
        static const char *const stages[4] = {"A", "D", "S", "R"};
        int column;
        int row;
        lv_obj_set_pos(hint, 20, 180);
        amp_envelope_module = lv_obj_create(pages[PAGE_AMP]);
        lv_obj_set_pos(amp_envelope_module, 178, 154);
        lv_obj_set_size(amp_envelope_module, 420, 76);
        lv_obj_set_style_radius(amp_envelope_module, 5, 0);
        lv_obj_set_style_bg_color(amp_envelope_module, col(0x06110f), 0);
        lv_obj_set_style_bg_grad_color(amp_envelope_module, col(0x0b211d), 0);
        lv_obj_set_style_bg_grad_dir(amp_envelope_module, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(amp_envelope_module, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(amp_envelope_module, 2, 0);
        lv_obj_set_style_border_color(amp_envelope_module, col(0x27574e), 0);
        lv_obj_set_style_border_opa(amp_envelope_module, LV_OPA_80, 0);
        lv_obj_set_style_shadow_width(amp_envelope_module, 7, 0);
        lv_obj_set_style_shadow_color(amp_envelope_module, col(0x2cb9a3), 0);
        lv_obj_set_style_shadow_opa(amp_envelope_module, LV_OPA_20, 0);
        lv_obj_set_style_pad_all(amp_envelope_module, 0, 0);
        lv_obj_remove_flag(amp_envelope_module, LV_OBJ_FLAG_CLICKABLE);
        no_scroll(amp_envelope_module);
        if(is_metal_theme()) {
            lv_obj_set_style_radius(amp_envelope_module, 2, 0);
            lv_obj_set_style_bg_color(amp_envelope_module, lv_color_hex(0x050d0f), 0);
            lv_obj_set_style_bg_grad_color(amp_envelope_module, lv_color_hex(0x092426), 0);
            lv_obj_set_style_border_width(amp_envelope_module, 4, 0);
            lv_obj_set_style_border_color(amp_envelope_module, lv_color_hex(0x77858c), 0);
            lv_obj_set_style_outline_width(amp_envelope_module, 2, 0);
            lv_obj_set_style_outline_color(amp_envelope_module, lv_color_hex(0x090d0f), 0);
            lv_obj_set_style_outline_pad(amp_envelope_module, 1, 0);
            lv_obj_set_style_shadow_color(amp_envelope_module, lv_color_hex(0x020405), 0);
            lv_obj_set_style_shadow_opa(amp_envelope_module, LV_OPA_80, 0);
        } else if(is_neon_theme()) {
            lv_obj_set_style_radius(amp_envelope_module, 2, 0);
            lv_obj_set_style_bg_color(amp_envelope_module, lv_color_hex(0x02090a), 0);
            lv_obj_set_style_bg_grad_color(amp_envelope_module, lv_color_hex(0x062126), 0);
            lv_obj_set_style_border_width(amp_envelope_module, 3, 0);
            lv_obj_set_style_border_color(amp_envelope_module, lv_color_hex(0x4f676c), 0);
            lv_obj_set_style_outline_width(amp_envelope_module, 2, 0);
            lv_obj_set_style_outline_color(amp_envelope_module, lv_color_hex(NEON_MAGENTA), 0);
            lv_obj_set_style_outline_opa(amp_envelope_module, LV_OPA_50, 0);
            lv_obj_set_style_outline_pad(amp_envelope_module, 1, 0);
            lv_obj_set_style_shadow_color(amp_envelope_module, lv_color_hex(0x010304), 0);
            lv_obj_set_style_shadow_opa(amp_envelope_module, LV_OPA_80, 0);
        }
        {
            lv_obj_t *inner_glass = lv_obj_create(amp_envelope_module);
            lv_obj_set_pos(inner_glass, 3, 3);
            lv_obj_set_size(inner_glass, 410, 66);
            lv_obj_set_style_radius(inner_glass, 3, 0);
            lv_obj_set_style_bg_opa(inner_glass, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(inner_glass, 1, 0);
            lv_obj_set_style_border_color(inner_glass, col(0x173b35), 0);
            lv_obj_set_style_border_opa(inner_glass, LV_OPA_70, 0);
            lv_obj_set_style_pad_all(inner_glass, 0, 0);
            lv_obj_remove_flag(inner_glass, LV_OBJ_FLAG_CLICKABLE);
            no_scroll(inner_glass);
        }
        for(column = 0; column < AMP_ENV_COLS; ++column) {
            amp_envelope_active_rows[column] = -1;
            for(row = 0; row < AMP_ENV_ROWS; ++row) {
                lv_obj_t *dot = lv_obj_create(amp_envelope_module);
                lv_obj_set_pos(dot, 9 + column * 10, 7 + row * 6);
                lv_obj_set_size(dot, 4, 4);
                lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_border_width(dot, 0, 0);
                lv_obj_set_style_pad_all(dot, 0, 0);
                lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
                no_scroll(dot);
                style_amp_envelope_dot(dot, false);
                amp_envelope_dots[column][row] = dot;
            }
        }
        for(i = 0; i < 4; ++i) {
            amp_stage_labels[i] = make_label(amp_envelope_module, stages[i], &lv_font_montserrat_12, 0x4d9f91);
            lv_obj_set_pos(amp_stage_labels[i], 0, 56);
            lv_obj_set_width(amp_stage_labels[i], 20);
            lv_obj_set_style_text_align(amp_stage_labels[i], LV_TEXT_ALIGN_CENTER, 0);
            if(is_neon_theme()) {
                const uint32_t stage_colors[4] = {NEON_CYAN, NEON_MAGENTA, NEON_LIME, NEON_RED};
                lv_obj_set_style_text_color(amp_stage_labels[i], lv_color_hex(stage_colors[i]), 0);
            }
        }
    }
}

static void create_fx_page(lv_obj_t *parent)
{
    static const char *const effects[FX_COUNT] = {"DELAY", "CHORUS", "DRIVE", "CRUSH"};
    int i;
    pages[PAGE_FX] = create_page(parent);
    for(i = 0; i < FX_COUNT; ++i) effect_buttons[i] = make_button(pages[PAGE_FX], effects[i], 12 + i * 151, 9, 140, 44, effect_event, i);
    for(i = 0; i < 3; ++i) {
        create_rotary_knob(pages[PAGE_FX], &fx_knobs[i], 57 + i * 201, 67,
                           ROTARY_FX, (uint8_t)i, "PARAM");
    }
    {
        lv_obj_t *note = make_label(pages[PAGE_FX], "CLICK EFFECT TO TOGGLE ON / OFF", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(note, 38, 224);
        lv_obj_t *chain = make_label(pages[PAGE_FX], "DELAY  >  CHORUS  >  DRIVE  >  CRUSH", &lv_font_montserrat_12, C_RED);
        lv_obj_set_pos(chain, 295, 224);
    }
}

static void create_lfo_page(lv_obj_t *parent)
{
    static const char *const wave_names[4] = {"SINE", "TRI", "SQR", "RND"};
    int i;
    pages[PAGE_LFO] = create_page(parent);
    lfo_target_label = make_label(pages[PAGE_LFO], "TARGET VOL", &lv_font_montserrat_18, C_TEXT);
    lv_obj_set_pos(lfo_target_label, 18, 13);
    lfo_power_button = make_button(pages[PAGE_LFO], "LFO OFF", 458, 8, 142, 42, lfo_power_event, 0);
    lfo_state_label = make_label(pages[PAGE_LFO], "RAT 00   DEP 00   WAV SINE", &lv_font_montserrat_14, C_CYAN);
    lv_obj_set_pos(lfo_state_label, 18, 60);
    create_rotary_knob(pages[PAGE_LFO], &lfo_knobs[0], 57, 88, ROTARY_LFO, 0, "RAT");
    create_rotary_knob(pages[PAGE_LFO], &lfo_knobs[1], 258, 88, ROTARY_LFO, 1, "DEP");
    {
        lv_obj_t *wave_label = make_label(pages[PAGE_LFO], "WAVEFORM", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(wave_label, 453, 84);
    }
    for(i = 0; i < 4; ++i) {
        int column = i % 2;
        int row = i / 2;
        lfo_wave_buttons[i] = make_button(pages[PAGE_LFO], wave_names[i],
                                          432 + column * 62, 104 + row * 60, 56, 56,
                                          lfo_wave_event, i);
    }
    {
        lv_obj_t *note = make_label(pages[PAGE_LFO], "CYAN CONTROLS EDIT LFO; AMBER CONTROLS EDIT BASE", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(note, 18, 226);
    }
}

static void create_bank_page(lv_obj_t *parent)
{
    static const char *const names[16] = {
        "GTR", "PNO", "ORG", "REC", "PAD", "PLK", "BEL", "BRS",
        "BAS", "SYN", "RND", "M1", "M2", "M3", "M4", "M5"
    };
    int i;
    pages[PAGE_BANK] = create_page(parent);
    for(i = 0; i < 16; ++i) {
        int row = i / 8;
        int column = i % 8;
        bank_buttons[i] = make_button(pages[PAGE_BANK], names[i], 13 + column * 76, 18 + row * 78, 68, 60, bank_event, i);
    }
    {
        lv_obj_t *note = make_label(pages[PAGE_BANK], "M1-M5 STORE THE CURRENT SLOT BEFORE SWITCHING", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(note, 14, 193);
    }
}

static void create_editor(lv_obj_t *screen)
{
    static const char *const names[PAGE_COUNT] = {"AMP", "FX", "LFO", "BANK"};
    lv_obj_t *editor = make_panel(screen, 14, 128, 650, 410);
    int i;
    add_screws(editor, 650, 410);
    for(i = 0; i < PAGE_COUNT; ++i) page_buttons[i] = make_button(editor, names[i], 18 + i * 117, 10, 108, 34, page_event, i);
    create_amp_page(editor);
    create_fx_page(editor);
    create_lfo_page(editor);
    create_bank_page(editor);

    make_rect(editor, 14, 314, 622, 2, C_EDGE);
    if(is_metal_theme()) {
        lv_obj_t *engraved_rule = make_rect(editor, 14, 314, 622, 1, 0x555345);
        lv_obj_set_style_bg_color(engraved_rule, lv_color_hex(0xa1abb0), 0);
        lv_obj_set_style_bg_opa(engraved_rule, LV_OPA_40, 0);
        make_rect(editor, 14, 315, 622, 1, 0x030302);
    } else if(is_neon_theme()) {
        make_neon_rect(editor, 14, 314, 160, 2, NEON_CYAN, LV_OPA_COVER, 8);
        make_neon_rect(editor, 178, 314, 96, 2, NEON_MAGENTA, LV_OPA_COVER, 8);
        make_neon_rect(editor, 278, 314, 170, 2, NEON_LIME, LV_OPA_COVER, 8);
        make_neon_rect(editor, 452, 314, 184, 2, NEON_AMBER, LV_OPA_COVER, 8);
    }
    target_label = make_label(editor, "BASE / VOL", &lv_font_montserrat_12, C_AMBER);
    lv_obj_set_pos(target_label, 22, 326);
    lv_obj_set_width(target_label, 500);
    target_value = make_label(editor, "72", &lv_font_montserrat_18, C_TEXT);
    lv_obj_set_pos(target_value, 540, 320);
    lv_obj_set_width(target_value, 80);
    lv_obj_set_style_text_align(target_value, LV_TEXT_ALIGN_RIGHT, 0);
    {
        int tick;
        lv_obj_t *slot = make_rect(editor, 24, 366, 596, 10, 0x090907);
        lv_obj_set_style_radius(slot, 3, 0);
        lv_obj_set_style_border_width(slot, 2, 0);
        lv_obj_set_style_border_color(slot, col(0x39372e), 0);
        lv_obj_set_style_shadow_width(slot, 5, 0);
        lv_obj_set_style_shadow_ofs_y(slot, 2, 0);
        lv_obj_set_style_shadow_color(slot, col(0x030302), 0);
        lv_obj_set_style_shadow_opa(slot, LV_OPA_80, 0);
        if(is_metal_theme()) {
            lv_obj_set_style_radius(slot, 1, 0);
            lv_obj_set_style_bg_color(slot, lv_color_hex(0x05080a), 0);
            lv_obj_set_style_border_width(slot, 3, 0);
            lv_obj_set_style_border_color(slot, lv_color_hex(0x5f6c73), 0);
            lv_obj_set_style_outline_width(slot, 1, 0);
            lv_obj_set_style_outline_color(slot, lv_color_hex(0xa4afb4), 0);
            lv_obj_set_style_outline_pad(slot, 0, 0);
            lv_obj_set_style_shadow_width(slot, 7, 0);
            lv_obj_set_style_shadow_ofs_y(slot, 3, 0);
        } else if(is_neon_theme()) {
            lv_obj_set_style_radius(slot, 1, 0);
            lv_obj_set_style_bg_color(slot, lv_color_hex(0x020607), 0);
            lv_obj_set_style_border_width(slot, 3, 0);
            lv_obj_set_style_border_color(slot, lv_color_hex(0x31474c), 0);
            lv_obj_set_style_outline_width(slot, 1, 0);
            lv_obj_set_style_outline_color(slot, lv_color_hex(NEON_MAGENTA), 0);
            lv_obj_set_style_outline_opa(slot, LV_OPA_40, 0);
            lv_obj_set_style_shadow_width(slot, 7, 0);
            lv_obj_set_style_shadow_ofs_y(slot, 3, 0);
        }
        for(tick = 0; tick < 11; ++tick) {
            int x = 43 + tick * 556 / 10;
            int h = tick == 0 || tick == 5 || tick == 10 ? 7 : 4;
            make_rect(editor, x, 356, 2, h, tick == 0 || tick == 10 ? C_BRASS : C_LINE);
            make_rect(editor, x, 381 - h, 2, h, tick == 0 || tick == 10 ? C_BRASS : C_LINE);
        }
        if(is_neon_theme()) {
            for(tick = 0; tick < 24; ++tick) {
                const uint32_t rails[5] = {NEON_CYAN, NEON_MAGENTA, NEON_LIME, NEON_AMBER, NEON_RED};
                lv_obj_t *segment = make_rect(editor, 30 + tick * 24, 369, 17, 3, rails[tick * 5 / 24]);
                lv_obj_set_style_bg_opa(segment, LV_OPA_70, 0);
            }
        }
    }
    common_slider = lv_slider_create(editor);
    lv_obj_set_pos(common_slider, 24, 351);
    lv_obj_set_size(common_slider, 596, 40);
    lv_slider_set_range(common_slider, 0, 100);
    lv_obj_set_style_bg_opa(common_slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(common_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(common_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(common_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_outline_opa(common_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(common_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(common_slider, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(common_slider, slider_event, LV_EVENT_VALUE_CHANGED, NULL);
    fader_cap = lv_obj_create(editor);
    lv_obj_set_pos(fader_cap, 24, 357);
    lv_obj_set_size(fader_cap, 40, 27);
    lv_obj_set_style_radius(fader_cap, 2, 0);
    lv_obj_set_style_bg_color(fader_cap, col(0xf2e7c8), 0);
    lv_obj_set_style_bg_grad_color(fader_cap, col(0x9e9278), 0);
    lv_obj_set_style_bg_grad_dir(fader_cap, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(fader_cap, 2, 0);
    lv_obj_set_style_border_color(fader_cap, col(0x554d3e), 0);
    lv_obj_set_style_shadow_width(fader_cap, 8, 0);
    lv_obj_set_style_shadow_ofs_y(fader_cap, 4, 0);
    lv_obj_set_style_shadow_color(fader_cap, col(0x030302), 0);
    lv_obj_set_style_shadow_opa(fader_cap, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(fader_cap, 0, 0);
    lv_obj_remove_flag(fader_cap, LV_OBJ_FLAG_CLICKABLE);
    no_scroll(fader_cap);
    if(is_metal_theme()) {
        lv_obj_set_style_radius(fader_cap, 2, 0);
        lv_obj_set_style_bg_color(fader_cap, lv_color_hex(0xc8d0d3), 0);
        lv_obj_set_style_bg_grad_color(fader_cap, lv_color_hex(0x556269), 0);
        lv_obj_set_style_border_color(fader_cap, lv_color_hex(0xe4eaec), 0);
        lv_obj_set_style_outline_width(fader_cap, 1, 0);
        lv_obj_set_style_outline_color(fader_cap, lv_color_hex(0x1a2125), 0);
        lv_obj_set_style_outline_pad(fader_cap, 1, 0);
        lv_obj_set_style_shadow_width(fader_cap, 10, 0);
        lv_obj_set_style_shadow_ofs_y(fader_cap, 5, 0);
        lv_obj_set_style_shadow_opa(fader_cap, LV_OPA_80, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(fader_cap, lv_color_hex(0x3d4b50), 0);
        lv_obj_set_style_bg_grad_color(fader_cap, lv_color_hex(0x0a0f11), 0);
        lv_obj_set_style_border_color(fader_cap, lv_color_hex(0x718186), 0);
        lv_obj_set_style_outline_width(fader_cap, 1, 0);
        lv_obj_set_style_outline_color(fader_cap, lv_color_hex(NEON_MAGENTA), 0);
        lv_obj_set_style_outline_opa(fader_cap, LV_OPA_60, 0);
        lv_obj_set_style_outline_pad(fader_cap, 1, 0);
        lv_obj_set_style_shadow_width(fader_cap, 10, 0);
        lv_obj_set_style_shadow_ofs_y(fader_cap, 5, 0);
        lv_obj_set_style_shadow_opa(fader_cap, LV_OPA_80, 0);
    }
    {
        lv_obj_t *bevel = make_rect(fader_cap, 4, 3, 28, 3, C_TEXT);
        lv_obj_t *groove_a = make_rect(fader_cap, 12, 8, 2, 13, 0x6d6554);
        lv_obj_t *groove_b = make_rect(fader_cap, 24, 8, 2, 13, 0x6d6554);
        lv_obj_set_style_bg_opa(bevel, LV_OPA_50, 0);
        lv_obj_set_style_border_width(groove_a, 1, 0);
        lv_obj_set_style_border_width(groove_b, 1, 0);
        lv_obj_set_style_border_color(groove_a, col(0xfaf0d4), 0);
        lv_obj_set_style_border_color(groove_b, col(0xfaf0d4), 0);
        if(is_metal_theme()) {
            lv_obj_set_style_bg_color(bevel, lv_color_hex(0xf1f4f5), 0);
            lv_obj_set_style_bg_opa(bevel, LV_OPA_50, 0);
            lv_obj_set_style_bg_color(groove_a, lv_color_hex(0x344047), 0);
            lv_obj_set_style_bg_color(groove_b, lv_color_hex(0x344047), 0);
            lv_obj_set_style_border_color(groove_a, lv_color_hex(0xaeb9be), 0);
            lv_obj_set_style_border_color(groove_b, lv_color_hex(0xaeb9be), 0);
        } else if(is_neon_theme()) {
            lv_obj_set_style_bg_color(bevel, lv_color_hex(0x7e9095), 0);
            lv_obj_set_style_bg_opa(bevel, LV_OPA_30, 0);
            lv_obj_set_style_bg_color(groove_a, lv_color_hex(0x050809), 0);
            lv_obj_set_style_bg_color(groove_b, lv_color_hex(0x050809), 0);
            lv_obj_set_style_border_color(groove_a, lv_color_hex(NEON_CYAN), 0);
            lv_obj_set_style_border_color(groove_b, lv_color_hex(NEON_LIME), 0);
        }
    }
}

static void create_xy(lv_obj_t *screen)
{
    lv_obj_t *container = make_panel(screen, 672, 128, 594, 410);
    lv_obj_t *pad;
    int i;
    add_screws(container, 594, 410);
    {
        lv_obj_t *title = make_label(container, "02  PERFORMANCE XY", &lv_font_montserrat_14, C_TEXT);
        lv_obj_set_pos(title, 18, 12);
        xy_readout = make_label(container, "MIDI 72.0  /  CONTINUOUS", &lv_font_montserrat_12, C_AMBER);
        lv_obj_set_pos(xy_readout, 330, 14);
    }
    pad = lv_obj_create(container);
    xy_pad = pad;
    lv_obj_set_pos(pad, 14, 40);
    lv_obj_set_size(pad, 566, 304);
    lv_obj_set_style_bg_color(pad, col(0x0d100d), 0);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pad, 2, 0);
    lv_obj_set_style_border_color(pad, col(C_BRASS), 0);
    lv_obj_set_style_radius(pad, 2, 0);
    lv_obj_set_style_pad_all(pad, 0, 0);
    no_scroll(pad);
    if(is_metal_theme()) {
        lv_obj_set_style_radius(pad, 1, 0);
        lv_obj_set_style_bg_color(pad, lv_color_hex(0x081012), 0);
        lv_obj_set_style_bg_grad_color(pad, lv_color_hex(0x121d20), 0);
        lv_obj_set_style_bg_grad_dir(pad, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(pad, 4, 0);
        lv_obj_set_style_border_color(pad, lv_color_hex(0x7e8b91), 0);
        lv_obj_set_style_outline_width(pad, 2, 0);
        lv_obj_set_style_outline_color(pad, lv_color_hex(0x06090b), 0);
        lv_obj_set_style_outline_pad(pad, 1, 0);
        lv_obj_set_style_shadow_width(pad, 8, 0);
        lv_obj_set_style_shadow_ofs_y(pad, 3, 0);
        lv_obj_set_style_shadow_color(pad, lv_color_hex(0x020304), 0);
        lv_obj_set_style_shadow_opa(pad, LV_OPA_70, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_radius(pad, 1, 0);
        lv_obj_set_style_bg_color(pad, lv_color_hex(0x02090b), 0);
        lv_obj_set_style_bg_grad_color(pad, lv_color_hex(0x0a1a1d), 0);
        lv_obj_set_style_bg_grad_dir(pad, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(pad, 5, 0);
        lv_obj_set_style_border_color(pad, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_outline_width(pad, 4, 0);
        lv_obj_set_style_outline_color(pad, lv_color_hex(NEON_MAGENTA), 0);
        lv_obj_set_style_outline_opa(pad, LV_OPA_COVER, 0);
        lv_obj_set_style_outline_pad(pad, 1, 0);
        lv_obj_set_style_shadow_width(pad, 28, 0);
        lv_obj_set_style_shadow_ofs_y(pad, 0, 0);
        lv_obj_set_style_shadow_color(pad, lv_color_hex(NEON_CYAN), 0);
        lv_obj_set_style_shadow_opa(pad, LV_OPA_70, 0);
    }
    for(i = 1; i < 4; ++i) {
        lv_obj_t *vertical = make_rect(pad, i * 141, 0, 1, 304, 0x3d4035);
        lv_obj_t *horizontal = make_rect(pad, 0, i * 76, 566, 1, 0x3d4035);
        if(is_neon_theme()) {
            lv_obj_set_style_bg_color(vertical, lv_color_hex(NEON_CYAN), 0);
            lv_obj_set_style_bg_color(horizontal, lv_color_hex(NEON_MAGENTA), 0);
            lv_obj_set_style_bg_opa(vertical, LV_OPA_30, 0);
            lv_obj_set_style_bg_opa(horizontal, LV_OPA_30, 0);
        }
    }
    xy_marker = lv_obj_create(pad);
    lv_obj_set_pos(xy_marker, 273, 142);
    lv_obj_set_size(xy_marker, 20, 20);
    lv_obj_set_style_radius(xy_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(xy_marker, col(C_RED), 0);
    lv_obj_set_style_bg_opa(xy_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(xy_marker, 2, 0);
    lv_obj_set_style_border_color(xy_marker, col(C_TEXT), 0);
    lv_obj_set_style_shadow_width(xy_marker, 9, 0);
    lv_obj_set_style_shadow_color(xy_marker, col(C_RED), 0);
    if(is_neon_theme()) {
        lv_obj_set_style_bg_color(xy_marker, lv_color_hex(0x071012), 0);
        lv_obj_set_style_border_color(xy_marker, lv_color_hex(0xe5f6ac), 0);
        lv_obj_set_style_shadow_color(xy_marker, lv_color_hex(NEON_LIME), 0);
        lv_obj_set_style_shadow_width(xy_marker, 6, 0);
        lv_obj_set_style_shadow_opa(xy_marker, LV_OPA_40, 0);
        make_rect(xy_marker, 2, 8, 14, 2, NEON_LIME);
        make_rect(xy_marker, 8, 2, 2, 14, NEON_AMBER);
    }
    lv_obj_remove_flag(xy_marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(xy_marker, LV_OBJ_FLAG_HIDDEN);
    xy_marker_visual_dirty = false;
    xy_readout_visual_dirty = false;
    lv_obj_add_event_cb(pad, xy_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(pad, xy_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(pad, xy_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(pad, xy_event, LV_EVENT_PRESS_LOST, NULL);
    mode_buttons[0] = make_button(container, "SEMITONE", 14, 356, 150, 38, mode_event, 0);
    mode_buttons[1] = make_button(container, "CONTINUOUS", 174, 356, 164, 38, mode_event, 1);
    {
        lv_obj_t *axis = make_label(container, "X PITCH       Y POSITION", &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(axis, 382, 369);
    }
}

static void create_keyboard(lv_obj_t *screen)
{
    static const int black_after[10] = {0, 1, 3, 4, 5, 7, 8, 10, 11, 12};
    static const int white_semitones[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int black_semitones[5] = {1, 3, 6, 8, 10};
    lv_obj_t *keyboard = make_panel(screen, 14, 545, 1252, 162);
    const int white_width = 81;
    int i;
    keyboard_panel = keyboard;
#if defined(LVGL_SYNTH_EMBEDDED)
    for(i = 0; i < 5; ++i) {
        keyboard_gestures[i].indev = NULL;
        keyboard_gestures[i].midi_note = -1;
    }
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_CLICKABLE);
#endif
    lv_obj_set_style_bg_color(keyboard, col(0x0c0d0b), 0);
    if(is_metal_theme()) {
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x242d32), 0);
        lv_obj_set_style_bg_grad_color(keyboard, lv_color_hex(0x0f1519), 0);
        lv_obj_set_style_bg_grad_dir(keyboard, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x929ea4), 0);
        lv_obj_set_style_outline_width(keyboard, 1, 0);
        lv_obj_set_style_outline_color(keyboard, lv_color_hex(0x05080a), 0);
        lv_obj_set_style_outline_pad(keyboard, 1, 0);
    } else if(is_neon_theme()) {
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x101719), 0);
        lv_obj_set_style_bg_grad_color(keyboard, lv_color_hex(0x06090b), 0);
        lv_obj_set_style_bg_grad_dir(keyboard, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x4d6166), 0);
        lv_obj_set_style_outline_width(keyboard, 3, 0);
        lv_obj_set_style_outline_color(keyboard, lv_color_hex(NEON_MAGENTA), 0);
        lv_obj_set_style_outline_opa(keyboard, LV_OPA_COVER, 0);
        lv_obj_set_style_outline_pad(keyboard, 1, 0);
        lv_obj_set_style_shadow_width(keyboard, 22, 0);
        lv_obj_set_style_shadow_ofs_y(keyboard, 0, 0);
        lv_obj_set_style_shadow_color(keyboard, lv_color_hex(NEON_MAGENTA), 0);
        lv_obj_set_style_shadow_opa(keyboard, LV_OPA_50, 0);
    }
    add_screws(keyboard, 1252, 162);
    {
        lv_obj_t *key_bed = make_rect(keyboard, 13, 10, 1226, 143, 0x070806);
        if(is_metal_theme()) {
            lv_obj_set_style_bg_color(key_bed, lv_color_hex(0x05080a), 0);
            lv_obj_set_style_border_width(key_bed, 2, 0);
            lv_obj_set_style_border_color(key_bed, lv_color_hex(0x66747b), 0);
            lv_obj_set_style_shadow_width(key_bed, 6, 0);
            lv_obj_set_style_shadow_ofs_y(key_bed, 2, 0);
            lv_obj_set_style_shadow_color(key_bed, lv_color_hex(0x010203), 0);
            make_rect(key_bed, 5, 3, 260, 3, NEON_CYAN);
            make_rect(key_bed, 270, 3, 180, 3, NEON_MAGENTA);
            make_rect(key_bed, 455, 3, 310, 3, NEON_LIME);
            make_rect(key_bed, 770, 3, 220, 3, NEON_AMBER);
            make_rect(key_bed, 995, 3, 220, 3, NEON_RED);
        } else if(is_neon_theme()) {
            lv_obj_set_style_bg_color(key_bed, lv_color_hex(0x020405), 0);
            lv_obj_set_style_border_width(key_bed, 2, 0);
            lv_obj_set_style_border_color(key_bed, lv_color_hex(0x3c5055), 0);
            lv_obj_set_style_shadow_width(key_bed, 7, 0);
            lv_obj_set_style_shadow_ofs_y(key_bed, 3, 0);
            lv_obj_set_style_shadow_color(key_bed, lv_color_hex(0x010203), 0);
            make_neon_rect(key_bed, 5, 3, 260, 2, NEON_CYAN, LV_OPA_COVER, 8);
            make_neon_rect(key_bed, 270, 3, 180, 2, NEON_MAGENTA, LV_OPA_COVER, 8);
            make_neon_rect(key_bed, 455, 3, 310, 2, NEON_LIME, LV_OPA_COVER, 8);
            make_neon_rect(key_bed, 770, 3, 220, 2, NEON_AMBER, LV_OPA_COVER, 8);
            make_neon_rect(key_bed, 995, 3, 220, 2, NEON_RED, LV_OPA_COVER, 8);
        }
    }
    for(i = 0; i < 15; ++i) {
        piano_key_t *key_state = &white_keys[i];
        lv_obj_t *key = lv_button_create(keyboard);
        key_state->obj = key;
        key_state->x = (int16_t)(17 + i * white_width);
        key_state->y = 13;
        key_state->width = white_width - 3;
        key_state->height = 136;
        key_state->press_progress = 0;
        key_state->visual_dirty = false;
        key_state->midi_note = (int16_t)(48 + (i / 7) * 12 + white_semitones[i % 7]);
        key_state->black = false;
        lv_obj_set_pos(key, 17 + i * white_width, 13);
        lv_obj_set_size(key, white_width - 3, 136);
        lv_obj_set_style_radius(key, 1, 0);
        lv_obj_set_style_bg_color(key, col(0xe8ddbd), 0);
        lv_obj_set_style_border_width(key, 2, 0);
        lv_obj_set_style_border_color(key, col(0x766e58), 0);
        lv_obj_set_style_shadow_width(key, 3, 0);
        lv_obj_set_style_shadow_ofs_y(key, 3, 0);
        lv_obj_set_style_shadow_color(key, col(0x050504), 0);
        lv_obj_set_style_shadow_opa(key, LV_OPA_60, 0);
        if(is_metal_theme()) {
            lv_obj_set_style_bg_color(key, lv_color_hex(0xe1e6e8), 0);
            lv_obj_set_style_bg_grad_color(key, lv_color_hex(0xaeb8bd), 0);
            lv_obj_set_style_bg_grad_dir(key, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(key, lv_color_hex(0x69767c), 0);
            lv_obj_set_style_shadow_width(key, 4, 0);
            lv_obj_set_style_shadow_ofs_y(key, 4, 0);
            lv_obj_set_style_shadow_opa(key, LV_OPA_80, 0);
        } else if(is_neon_theme()) {
            lv_obj_set_style_bg_color(key, lv_color_hex(0xc8d0cc), 0);
            lv_obj_set_style_bg_grad_color(key, lv_color_hex(0x869397), 0);
            lv_obj_set_style_bg_grad_dir(key, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(key, lv_color_hex(0x54666b), 0);
            lv_obj_set_style_shadow_width(key, 4, 0);
            lv_obj_set_style_shadow_ofs_y(key, 4, 0);
            lv_obj_set_style_shadow_opa(key, LV_OPA_80, 0);
        }
#if defined(LVGL_SYNTH_EMBEDDED)
        lv_obj_remove_flag(key, LV_OBJ_FLAG_CLICKABLE);
#else
        lv_obj_add_event_cb(key, key_event, LV_EVENT_PRESSED, key_state);
        lv_obj_add_event_cb(key, key_event, LV_EVENT_RELEASED, key_state);
        lv_obj_add_event_cb(key, key_event, LV_EVENT_PRESS_LOST, key_state);
#endif
    }
    for(i = 0; i < 10; ++i) {
        piano_key_t *key_state = &black_keys[i];
        lv_obj_t *key = lv_button_create(keyboard);
        key_state->obj = key;
        key_state->x = (int16_t)(17 + (black_after[i] + 1) * white_width - 25);
        key_state->y = 13;
        key_state->width = 49;
        key_state->height = 86;
        key_state->press_progress = 0;
        key_state->visual_dirty = false;
        key_state->midi_note = (int16_t)(48 + (i / 5) * 12 + black_semitones[i % 5]);
        key_state->black = true;
        lv_obj_set_pos(key, 17 + (black_after[i] + 1) * white_width - 25, 13);
        lv_obj_set_size(key, 49, 86);
        lv_obj_set_style_radius(key, 1, 0);
        lv_obj_set_style_bg_color(key, col(0x121310), 0);
        lv_obj_set_style_border_width(key, 2, 0);
        lv_obj_set_style_border_color(key, col(0x414036), 0);
        lv_obj_set_style_shadow_width(key, 8, 0);
        lv_obj_set_style_shadow_ofs_y(key, 5, 0);
        lv_obj_set_style_shadow_color(key, col(0x030302), 0);
        lv_obj_set_style_shadow_opa(key, LV_OPA_60, 0);
        if(is_metal_theme()) {
            lv_obj_set_style_bg_color(key, lv_color_hex(0x39444a), 0);
            lv_obj_set_style_bg_grad_color(key, lv_color_hex(0x0b1013), 0);
            lv_obj_set_style_bg_grad_dir(key, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(key, lv_color_hex(0x7b888e), 0);
            lv_obj_set_style_shadow_width(key, 9, 0);
            lv_obj_set_style_shadow_ofs_y(key, 6, 0);
            lv_obj_set_style_shadow_opa(key, LV_OPA_80, 0);
        } else if(is_neon_theme()) {
            lv_obj_set_style_bg_color(key, lv_color_hex(0x1c2528), 0);
            lv_obj_set_style_bg_grad_color(key, lv_color_hex(0x050809), 0);
            lv_obj_set_style_bg_grad_dir(key, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_color(key, lv_color_hex(0x5b6d72), 0);
            lv_obj_set_style_shadow_width(key, 9, 0);
            lv_obj_set_style_shadow_ofs_y(key, 6, 0);
            lv_obj_set_style_shadow_opa(key, LV_OPA_80, 0);
        }
#if defined(LVGL_SYNTH_EMBEDDED)
        lv_obj_remove_flag(key, LV_OBJ_FLAG_CLICKABLE);
#else
        lv_obj_add_event_cb(key, key_event, LV_EVENT_PRESSED, key_state);
        lv_obj_add_event_cb(key, key_event, LV_EVENT_RELEASED, key_state);
        lv_obj_add_event_cb(key, key_event, LV_EVENT_PRESS_LOST, key_state);
#endif
    }
#if defined(LVGL_SYNTH_EMBEDDED)
    lv_obj_add_event_cb(keyboard, keyboard_swipe_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(keyboard, keyboard_swipe_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(keyboard, keyboard_swipe_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(keyboard, keyboard_swipe_event, LV_EVENT_PRESS_LOST, NULL);
#endif
}

static void create_background(lv_obj_t *screen)
{
#if defined(LVGL_SYNTH_EMBEDDED)
    int x;
    make_rect(screen, 0, 0, 1280, 720, C_WOOD);
    for(x = 0; x < 1280; x += 80) {
        lv_obj_t *grain = make_rect(screen, x, 0, 3, 720, (x / 80) % 2 ? 0x6b3820 : 0x452112);
        lv_obj_set_style_bg_opa(grain, LV_OPA_30, 0);
    }
#else
    lv_obj_t *background = lv_image_create(screen);
    const char *source = "A:/assets/walnut-background.png";
    if(current_theme == LVGL_SYNTH_THEME_METAL) {
        source = "A:/assets/metal-hairline-background.png";
    } else if(current_theme == LVGL_SYNTH_THEME_NEON) {
        source = "A:/assets/neon-digital-background.png";
    }
    lv_image_set_src(background, source);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_set_size(background, 1280, 720);
    lv_obj_remove_flag(background, LV_OBJ_FLAG_CLICKABLE);
#endif
}

void lvgl_synth_ui_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    int i;
    if(!state_ready) {
        default_patch(&state);
        state_ready = true;
    }
    if(!memories_ready) {
        for(i = 0; i < 5; ++i) {
            default_patch(&memories[i]);
        }
        memories_ready = true;
    }
    lv_obj_set_style_bg_color(screen, col(C_WOOD), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    no_scroll(screen);
    create_background(screen);
    create_header(screen);
    create_sources(screen);
    create_editor(screen);
    create_xy(screen);
    create_keyboard(screen);
    create_theme_hitboxes(screen);
    refresh_ui();
}

void lvgl_synth_ui_invalidate_performance(void)
{
    if(xy_marker != NULL) lv_obj_invalidate(lv_obj_get_parent(xy_marker));
    if(keyboard_panel != NULL) lv_obj_invalidate(keyboard_panel);
}

void lvgl_synth_ui_invalidate_dirty_keys(void)
{
    lv_area_t keyboard_area;
    int i;
    if(keyboard_panel == NULL) return;
    lv_obj_get_content_coords(keyboard_panel, &keyboard_area);
    for(i = 0; i < 25; ++i) {
        piano_key_t *key = i < 15 ? &white_keys[i] : &black_keys[i - 15];
        lv_area_t area;
        if(!key->visual_dirty) continue;
        area.x1 = keyboard_area.x1 + key->x - 10;
        area.y1 = keyboard_area.y1 + key->y - 10;
        area.x2 = keyboard_area.x1 + key->x + key->width + 10;
        area.y2 = keyboard_area.y1 + key->y + key->height + 10;
        lv_obj_invalidate_area(keyboard_panel, &area);
        key->visual_dirty = false;
    }
}

void lvgl_synth_ui_invalidate_dirty_xy(void)
{
    if(xy_marker_visual_dirty) {
        // Rebuild the pad once from its final state. This guarantees that any
        // previously displayed marker is erased and only the latest one is drawn.
        lv_obj_invalidate(xy_pad);
        xy_marker_visual_dirty = false;
    }
    if(xy_readout_visual_dirty && xy_readout != NULL) {
        lv_obj_invalidate(xy_readout);
        xy_readout_visual_dirty = false;
    }
}
