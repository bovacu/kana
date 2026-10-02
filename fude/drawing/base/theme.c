#include "drawing/base/theme.h"

// ===========================================================================
// See theme.h. Each theme is one table; add a theme by adding a row here and a
// name to FUDE_THEME_.
// ===========================================================================

#define C(_r, _g, _b)       (rde_color){ _r, _g, _b, 255 }
#define CA(_r, _g, _b, _a)  (rde_color){ _r, _g, _b, _a }

static const fude_theme FUDE_THEMES[FUDE_THEME_COUNT] = {
    [FUDE_THEME_PAPER] = {
        .name = "Paper",
        .page = C(248, 247, 243), .page_dots = C(200, 198, 190), .text = C(46, 46, 54), .text_soft = C(111, 109, 102),
        .ink = C(30, 30, 36), .line = C(226, 223, 214), .hud = C(60, 60, 70),
        .ghost = C(216, 213, 204), .pen_tip = C(214, 60, 60), .stroke_number = C(200, 70, 70),
        .sheet = C(255, 255, 255), .sheet_outline = C(180, 176, 166), .sheet_guide = C(214, 210, 200), .reference = CA(220, 90, 90, 70),
        .score_good = C(46, 138, 78), .score_fair = C(183, 128, 28), .score_poor = C(192, 68, 60),
        .select = C(40, 120, 230), .select_glow = CA(80, 150, 255, 80), .select_fill = CA(80, 150, 255, 20),
        .eraser = C(200, 90, 90), .samples = C(40, 160, 220),
        .surface = C(255, 255, 255), .surface_2 = C(240, 238, 232), .outline = C(226, 222, 212),
        .accent = C(58, 108, 200), .on_accent = C(255, 255, 255), .tint = CA(58, 108, 200, 31),
        .panel = C(255, 255, 255), .panel_border = C(226, 222, 212), .grip = C(160, 157, 150),
        .button = C(240, 238, 232), .button_selected = C(58, 108, 200), .button_text = C(46, 46, 54), .button_text_disabled = C(182, 179, 172),
        .danger = C(192, 68, 60), .field = C(248, 247, 243), .field_border = C(208, 204, 194), .field_placeholder = C(111, 109, 102),
        .slider_track = C(226, 222, 212), .slider_fill = C(58, 108, 200), .slider_thumb = C(58, 108, 200), .swatch_border = C(226, 222, 212),
    },
    [FUDE_THEME_WASHI] = {
        .name = "Washi",
        .page = C(241, 233, 216), .page_dots = C(205, 192, 168), .text = C(70, 56, 44), .text_soft = C(124, 106, 85),
        .ink = C(45, 34, 26), .line = C(224, 212, 190), .hud = C(70, 56, 44),
        .ghost = C(218, 205, 182), .pen_tip = C(180, 64, 40), .stroke_number = C(170, 70, 50),
        .sheet = C(251, 246, 236), .sheet_outline = C(176, 160, 134), .sheet_guide = C(214, 200, 176), .reference = CA(190, 90, 60, 70),
        .score_good = C(79, 138, 60), .score_fair = C(173, 116, 36), .score_poor = C(180, 60, 50),
        .select = C(150, 90, 40), .select_glow = CA(200, 140, 80, 80), .select_fill = CA(200, 140, 80, 20),
        .eraser = C(180, 80, 60), .samples = C(60, 130, 170),
        .surface = C(251, 246, 236), .surface_2 = C(237, 226, 204), .outline = C(217, 201, 172),
        .accent = C(160, 83, 42), .on_accent = C(255, 255, 255), .tint = CA(160, 83, 42, 33),
        .panel = C(251, 246, 236), .panel_border = C(217, 201, 172), .grip = C(170, 152, 128),
        .button = C(237, 226, 204), .button_selected = C(160, 83, 42), .button_text = C(70, 56, 44), .button_text_disabled = C(192, 176, 152),
        .danger = C(180, 60, 50), .field = C(241, 233, 216), .field_border = C(200, 184, 156), .field_placeholder = C(124, 106, 85),
        .slider_track = C(217, 201, 172), .slider_fill = C(160, 83, 42), .slider_thumb = C(160, 83, 42), .swatch_border = C(217, 201, 172),
    },
    [FUDE_THEME_NIGHT] = {
        .name = "Night",
        .page = C(22, 23, 27), .page_dots = C(52, 54, 62), .text = C(230, 230, 236), .text_soft = C(154, 156, 166),
        .ink = C(236, 234, 228), .line = C(44, 46, 54), .hud = C(170, 172, 182),
        .ghost = C(58, 60, 68), .pen_tip = C(255, 110, 110), .stroke_number = C(255, 130, 120),
        .sheet = C(36, 38, 45), .sheet_outline = C(90, 92, 104), .sheet_guide = C(56, 58, 68), .reference = CA(255, 120, 120, 60),
        .score_good = C(90, 200, 120), .score_fair = C(230, 180, 70), .score_poor = C(240, 100, 100),
        .select = C(90, 160, 255), .select_glow = CA(90, 160, 255, 90), .select_fill = CA(90, 160, 255, 24),
        .eraser = C(230, 110, 110), .samples = C(80, 190, 240),
        .surface = C(36, 38, 45), .surface_2 = C(48, 50, 59), .outline = C(58, 60, 70),
        .accent = C(90, 140, 240), .on_accent = C(255, 255, 255), .tint = CA(90, 140, 240, 51),
        .panel = C(36, 38, 45), .panel_border = C(58, 60, 70), .grip = C(110, 112, 124),
        .button = C(48, 50, 59), .button_selected = C(90, 140, 240), .button_text = C(230, 230, 236), .button_text_disabled = C(96, 98, 110),
        .danger = C(240, 100, 100), .field = C(26, 27, 32), .field_border = C(78, 81, 94), .field_placeholder = C(154, 156, 166),
        .slider_track = C(58, 60, 70), .slider_fill = C(90, 140, 240), .slider_thumb = C(90, 140, 240), .swatch_border = C(58, 60, 70),
    },
    [FUDE_THEME_MATCHA] = {
        .name = "Matcha",
        .page = C(237, 242, 232), .page_dots = C(190, 204, 184), .text = C(44, 60, 48), .text_soft = C(102, 120, 104),
        .ink = C(26, 38, 30), .line = C(214, 226, 208), .hud = C(44, 60, 48),
        .ghost = C(206, 218, 200), .pen_tip = C(200, 80, 60), .stroke_number = C(60, 130, 80),
        .sheet = C(249, 251, 246), .sheet_outline = C(160, 178, 154), .sheet_guide = C(204, 218, 198), .reference = CA(120, 170, 110, 80),
        .score_good = C(47, 118, 68), .score_fair = C(169, 122, 34), .score_poor = C(180, 70, 60),
        .select = C(60, 140, 90), .select_glow = CA(90, 170, 110, 80), .select_fill = CA(90, 170, 110, 20),
        .eraser = C(190, 90, 70), .samples = C(50, 140, 170),
        .surface = C(249, 251, 246), .surface_2 = C(226, 234, 219), .outline = C(206, 218, 198),
        .accent = C(51, 120, 74), .on_accent = C(255, 255, 255), .tint = CA(51, 120, 74, 33),
        .panel = C(249, 251, 246), .panel_border = C(206, 218, 198), .grip = C(146, 162, 142),
        .button = C(226, 234, 219), .button_selected = C(51, 120, 74), .button_text = C(44, 60, 48), .button_text_disabled = C(170, 184, 166),
        .danger = C(180, 70, 60), .field = C(237, 242, 232), .field_border = C(186, 200, 178), .field_placeholder = C(102, 120, 104),
        .slider_track = C(206, 218, 198), .slider_fill = C(51, 120, 74), .slider_thumb = C(51, 120, 74), .swatch_border = C(206, 218, 198),
    },
    [FUDE_THEME_SAKURA] = {
        .name = "Sakura",
        .page = C(250, 241, 243), .page_dots = C(222, 198, 206), .text = C(72, 44, 56), .text_soft = C(134, 102, 111),
        .ink = C(48, 28, 38), .line = C(238, 222, 228), .hud = C(72, 44, 56),
        .ghost = C(234, 212, 220), .pen_tip = C(214, 70, 110), .stroke_number = C(200, 70, 110),
        .sheet = C(255, 250, 251), .sheet_outline = C(206, 176, 188), .sheet_guide = C(234, 214, 222), .reference = CA(220, 90, 130, 70),
        .score_good = C(60, 138, 88), .score_fair = C(173, 122, 36), .score_poor = C(196, 60, 80),
        .select = C(200, 80, 130), .select_glow = CA(230, 120, 160, 80), .select_fill = CA(230, 120, 160, 20),
        .eraser = C(200, 80, 90), .samples = C(70, 150, 200),
        .surface = C(255, 250, 251), .surface_2 = C(244, 228, 234), .outline = C(232, 204, 214),
        .accent = C(174, 63, 106), .on_accent = C(255, 255, 255), .tint = CA(174, 63, 106, 31),
        .panel = C(255, 250, 251), .panel_border = C(232, 204, 214), .grip = C(186, 150, 162),
        .button = C(244, 228, 234), .button_selected = C(174, 63, 106), .button_text = C(72, 44, 56), .button_text_disabled = C(206, 178, 188),
        .danger = C(196, 60, 80), .field = C(250, 241, 243), .field_border = C(222, 192, 202), .field_placeholder = C(134, 102, 111),
        .slider_track = C(232, 204, 214), .slider_fill = C(174, 63, 106), .slider_thumb = C(174, 63, 106), .swatch_border = C(232, 204, 214),
    },
};

#undef C
#undef CA

RDE_INTERNAL FUDE_THEME_ fude_theme_current = FUDE_THEME_PAPER;

const fude_theme* fude_theme_active(void) {
    return &FUDE_THEMES[fude_theme_current];
}

FUDE_THEME_ fude_theme_index(void) {
    return fude_theme_current;
}

void fude_theme_set(FUDE_THEME_ _theme) {
    fude_theme_current = (_theme >= 0 && _theme < FUDE_THEME_COUNT) ? _theme : FUDE_THEME_PAPER;
}

const fude_theme* fude_theme_get(FUDE_THEME_ _theme) {
    return &FUDE_THEMES[(_theme >= 0 && _theme < FUDE_THEME_COUNT) ? _theme : FUDE_THEME_PAPER];
}

b8 fude_theme_is_ink(rde_color _color) {
    return _color.a == 0;
}

rde_color fude_theme_resolve(rde_color _color) {
    return fude_theme_is_ink(_color) ? fude_theme_active()->ink : _color;
}

rde_color fude_theme_grade(f32 _score) {
    const fude_theme* _t = fude_theme_active();
    return _score >= FUDE_THEME_GRADE_GOOD ? _t->score_good : _score >= FUDE_THEME_GRADE_FAIR ? _t->score_fair : _t->score_poor;
}
