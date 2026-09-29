#include "theme.h"

// ===========================================================================
// See theme.h. Each theme is one table; add a theme by adding a row here and a
// name to KANA_THEME_.
// ===========================================================================

#define C(_r, _g, _b)       (rde_color){ _r, _g, _b, 255 }
#define CA(_r, _g, _b, _a)  (rde_color){ _r, _g, _b, _a }

static const kana_theme KANA_THEMES[KANA_THEME_COUNT] = {
    [KANA_THEME_PAPER] = {
        .name = "Paper",
        .page = C(248, 247, 243), .page_dots = C(200, 198, 190), .text = C(60, 60, 70), .text_soft = C(140, 138, 132),
        .ink = C(30, 30, 36), .line = C(226, 223, 214), .hud = C(60, 60, 70),
        .ghost = C(216, 213, 204), .pen_tip = C(214, 60, 60), .stroke_number = C(200, 70, 70),
        .sheet = C(255, 255, 255), .sheet_outline = C(180, 176, 166), .sheet_guide = C(214, 210, 200), .reference = CA(220, 90, 90, 70),
        .score_good = C(40, 150, 80), .score_fair = C(200, 140, 30), .score_poor = C(200, 60, 60),
        .select = C(40, 120, 230), .select_glow = CA(80, 150, 255, 80), .select_fill = CA(80, 150, 255, 20),
        .eraser = C(200, 90, 90), .samples = C(40, 160, 220),
        .panel = CA(34, 34, 40, 235), .panel_border = C(72, 72, 84), .grip = C(86, 86, 100),
        .button = C(54, 54, 62), .button_selected = C(58, 108, 200), .button_text = C(235, 235, 240), .button_text_disabled = C(120, 120, 130),
        .danger = C(190, 60, 60), .field = C(58, 58, 68), .field_placeholder = C(150, 150, 160),
        .slider_track = C(70, 70, 82), .slider_fill = C(90, 135, 220), .slider_thumb = C(230, 230, 236), .swatch_border = C(120, 120, 132),
    },
    [KANA_THEME_WASHI] = {
        .name = "Washi",
        .page = C(241, 233, 216), .page_dots = C(205, 192, 168), .text = C(70, 56, 44), .text_soft = C(140, 122, 100),
        .ink = C(45, 34, 26), .line = C(224, 212, 190), .hud = C(70, 56, 44),
        .ghost = C(218, 205, 182), .pen_tip = C(180, 64, 40), .stroke_number = C(170, 70, 50),
        .sheet = C(250, 245, 233), .sheet_outline = C(176, 160, 134), .sheet_guide = C(214, 200, 176), .reference = CA(190, 90, 60, 70),
        .score_good = C(70, 130, 60), .score_fair = C(190, 130, 40), .score_poor = C(180, 60, 50),
        .select = C(150, 90, 40), .select_glow = CA(200, 140, 80, 80), .select_fill = CA(200, 140, 80, 20),
        .eraser = C(180, 80, 60), .samples = C(60, 130, 170),
        .panel = CA(62, 50, 40, 240), .panel_border = C(100, 84, 68), .grip = C(120, 100, 82),
        .button = C(84, 70, 58), .button_selected = C(160, 96, 52), .button_text = C(245, 236, 220), .button_text_disabled = C(150, 136, 120),
        .danger = C(170, 60, 50), .field = C(84, 70, 58), .field_placeholder = C(170, 152, 132),
        .slider_track = C(100, 84, 68), .slider_fill = C(190, 120, 60), .slider_thumb = C(245, 236, 220), .swatch_border = C(140, 120, 100),
    },
    [KANA_THEME_NIGHT] = {
        .name = "Night",
        .page = C(22, 23, 27), .page_dots = C(52, 54, 62), .text = C(214, 214, 222), .text_soft = C(140, 142, 152),
        .ink = C(236, 234, 228), .line = C(44, 46, 54), .hud = C(170, 172, 182),
        .ghost = C(58, 60, 68), .pen_tip = C(255, 110, 110), .stroke_number = C(255, 130, 120),
        .sheet = C(32, 33, 39), .sheet_outline = C(90, 92, 104), .sheet_guide = C(56, 58, 68), .reference = CA(255, 120, 120, 60),
        .score_good = C(90, 200, 120), .score_fair = C(230, 180, 70), .score_poor = C(240, 100, 100),
        .select = C(90, 160, 255), .select_glow = CA(90, 160, 255, 90), .select_fill = CA(90, 160, 255, 24),
        .eraser = C(230, 110, 110), .samples = C(80, 190, 240),
        .panel = CA(44, 46, 54, 240), .panel_border = C(80, 82, 96), .grip = C(96, 98, 112),
        .button = C(60, 62, 72), .button_selected = C(70, 120, 220), .button_text = C(236, 236, 242), .button_text_disabled = C(110, 112, 124),
        .danger = C(190, 70, 70), .field = C(30, 31, 37), .field_placeholder = C(120, 122, 134),
        .slider_track = C(70, 72, 84), .slider_fill = C(90, 140, 230), .slider_thumb = C(230, 232, 240), .swatch_border = C(110, 112, 124),
    },
    [KANA_THEME_MATCHA] = {
        .name = "Matcha",
        .page = C(237, 242, 232), .page_dots = C(190, 204, 184), .text = C(44, 60, 48), .text_soft = C(116, 134, 118),
        .ink = C(26, 38, 30), .line = C(214, 226, 208), .hud = C(44, 60, 48),
        .ghost = C(206, 218, 200), .pen_tip = C(200, 80, 60), .stroke_number = C(60, 130, 80),
        .sheet = C(250, 253, 247), .sheet_outline = C(160, 178, 154), .sheet_guide = C(204, 218, 198), .reference = CA(120, 170, 110, 80),
        .score_good = C(50, 140, 70), .score_fair = C(190, 140, 40), .score_poor = C(190, 70, 60),
        .select = C(60, 140, 90), .select_glow = CA(90, 170, 110, 80), .select_fill = CA(90, 170, 110, 20),
        .eraser = C(190, 90, 70), .samples = C(50, 140, 170),
        .panel = CA(40, 60, 46, 240), .panel_border = C(72, 98, 78), .grip = C(92, 118, 98),
        .button = C(56, 80, 62), .button_selected = C(76, 140, 92), .button_text = C(236, 244, 236), .button_text_disabled = C(124, 146, 128),
        .danger = C(170, 60, 55), .field = C(56, 80, 62), .field_placeholder = C(150, 172, 154),
        .slider_track = C(72, 98, 78), .slider_fill = C(110, 170, 120), .slider_thumb = C(236, 244, 236), .swatch_border = C(110, 136, 114),
    },
    [KANA_THEME_SAKURA] = {
        .name = "Sakura",
        .page = C(250, 241, 243), .page_dots = C(222, 198, 206), .text = C(72, 44, 56), .text_soft = C(150, 118, 130),
        .ink = C(48, 28, 38), .line = C(238, 222, 228), .hud = C(72, 44, 56),
        .ghost = C(234, 212, 220), .pen_tip = C(214, 70, 110), .stroke_number = C(200, 70, 110),
        .sheet = C(255, 251, 252), .sheet_outline = C(206, 176, 188), .sheet_guide = C(234, 214, 222), .reference = CA(220, 90, 130, 70),
        .score_good = C(60, 150, 90), .score_fair = C(200, 140, 40), .score_poor = C(200, 60, 80),
        .select = C(200, 80, 130), .select_glow = CA(230, 120, 160, 80), .select_fill = CA(230, 120, 160, 20),
        .eraser = C(200, 80, 90), .samples = C(70, 150, 200),
        .panel = CA(86, 46, 62, 240), .panel_border = C(124, 78, 96), .grip = C(142, 96, 114),
        .button = C(108, 62, 82), .button_selected = C(200, 86, 128), .button_text = C(252, 238, 244), .button_text_disabled = C(170, 136, 150),
        .danger = C(180, 50, 70), .field = C(108, 62, 82), .field_placeholder = C(196, 160, 174),
        .slider_track = C(124, 78, 96), .slider_fill = C(222, 120, 160), .slider_thumb = C(252, 238, 244), .swatch_border = C(160, 116, 134),
    },
};

#undef C
#undef CA

RDE_INTERNAL KANA_THEME_ kana_theme_current = KANA_THEME_PAPER;

const kana_theme* kana_theme_active(void) {
    return &KANA_THEMES[kana_theme_current];
}

KANA_THEME_ kana_theme_index(void) {
    return kana_theme_current;
}

void kana_theme_set(KANA_THEME_ _theme) {
    kana_theme_current = (_theme >= 0 && _theme < KANA_THEME_COUNT) ? _theme : KANA_THEME_PAPER;
}

const kana_theme* kana_theme_get(KANA_THEME_ _theme) {
    return &KANA_THEMES[(_theme >= 0 && _theme < KANA_THEME_COUNT) ? _theme : KANA_THEME_PAPER];
}

b8 kana_theme_is_ink(rde_color _color) {
    return _color.a == 0;
}

rde_color kana_theme_resolve(rde_color _color) {
    return kana_theme_is_ink(_color) ? kana_theme_active()->ink : _color;
}

rde_color kana_theme_grade(f32 _score) {
    const kana_theme* _t = kana_theme_active();
    return _score >= KANA_THEME_GRADE_GOOD ? _t->score_good : _score >= KANA_THEME_GRADE_FAIR ? _t->score_fair : _t->score_poor;
}
