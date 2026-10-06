// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/themes.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"

#define C(_r, _g, _b)       (rde_color){ _r, _g, _b, 255 }
#define CA(_r, _g, _b, _a)  (rde_color){ _r, _g, _b, _a }

static const fude_theme FUDE_ZOOM_THEME_KRAFT = {
    .name = "Kraft",
    .page = C(226, 207, 172), .page_dots = C(190, 166, 126), .text = C(58, 42, 28), .text_soft = C(112, 90, 66),
    .ink = C(40, 28, 18), .line = C(206, 186, 150), .hud = C(58, 42, 28),
    .ghost = C(208, 188, 152), .pen_tip = C(180, 64, 40), .stroke_number = C(170, 70, 50),
    .sheet = C(240, 226, 198), .sheet_outline = C(170, 146, 108), .sheet_guide = C(204, 182, 146), .reference = CA(190, 90, 60, 70),
    .score_good = C(70, 128, 52), .score_fair = C(168, 110, 30), .score_poor = C(176, 58, 44),
    .select = C(168, 76, 26), .select_glow = CA(206, 120, 60, 80), .select_fill = CA(206, 120, 60, 22),
    .eraser = C(176, 70, 50), .samples = C(60, 120, 160),
    .surface = C(243, 232, 210), .surface_2 = C(230, 214, 184), .outline = C(206, 186, 150),
    .accent = C(176, 86, 30), .on_accent = C(255, 255, 255), .tint = CA(176, 86, 30, 36),
    .panel = C(243, 232, 210), .panel_border = C(206, 186, 150), .grip = C(160, 136, 100),
    .button = C(230, 214, 184), .button_selected = C(176, 86, 30), .button_text = C(58, 42, 28), .button_text_disabled = C(180, 160, 128),
    .danger = C(176, 58, 44), .field = C(236, 222, 194), .field_border = C(190, 166, 126), .field_placeholder = C(112, 90, 66),
    .slider_track = C(206, 186, 150), .slider_fill = C(176, 86, 30), .slider_thumb = C(176, 86, 30), .swatch_border = C(206, 186, 150),
};

static const fude_theme FUDE_ZOOM_THEME_BLUEPRINT = {
    .name = "Blueprint",
    .page = C(20, 62, 120), .page_dots = C(64, 108, 170), .text = C(232, 240, 252), .text_soft = C(168, 192, 226),
    .ink = C(244, 248, 255), .line = C(44, 90, 152), .hud = C(200, 216, 240),
    .ghost = C(52, 96, 156), .pen_tip = C(255, 214, 90), .stroke_number = C(255, 214, 90),
    .sheet = C(28, 74, 138), .sheet_outline = C(110, 150, 206), .sheet_guide = C(56, 102, 164), .reference = CA(255, 214, 90, 70),
    .score_good = C(120, 220, 150), .score_fair = C(255, 210, 90), .score_poor = C(255, 120, 110),
    .select = C(255, 214, 90), .select_glow = CA(255, 214, 90, 80), .select_fill = CA(255, 214, 90, 22),
    .eraser = C(255, 130, 120), .samples = C(130, 210, 255),
    .surface = C(24, 56, 104), .surface_2 = C(34, 72, 128), .outline = C(58, 98, 156),
    .accent = C(255, 204, 72), .on_accent = C(26, 40, 66), .tint = CA(255, 204, 72, 44),
    .panel = C(24, 56, 104), .panel_border = C(58, 98, 156), .grip = C(120, 150, 196),
    .button = C(34, 72, 128), .button_selected = C(255, 204, 72), .button_text = C(232, 240, 252), .button_text_disabled = C(100, 130, 176),
    .danger = C(255, 120, 110), .field = C(16, 48, 94), .field_border = C(84, 124, 184), .field_placeholder = C(150, 176, 214),
    .slider_track = C(58, 98, 156), .slider_fill = C(255, 204, 72), .slider_thumb = C(255, 204, 72), .swatch_border = C(58, 98, 156),
};

static const fude_theme FUDE_ZOOM_THEME_MAT = {
    .name = "Cutting mat",
    .page = C(30, 84, 66), .page_dots = C(88, 146, 120), .text = C(236, 244, 238), .text_soft = C(176, 204, 188),
    .ink = C(246, 250, 246), .line = C(54, 112, 90), .hud = C(210, 230, 218),
    .ghost = C(60, 118, 96), .pen_tip = C(255, 196, 80), .stroke_number = C(255, 196, 80),
    .sheet = C(38, 96, 76), .sheet_outline = C(120, 176, 150), .sheet_guide = C(66, 124, 102), .reference = CA(255, 196, 80, 70),
    .score_good = C(140, 230, 160), .score_fair = C(255, 206, 90), .score_poor = C(255, 126, 110),
    .select = C(255, 196, 80), .select_glow = CA(255, 196, 80, 80), .select_fill = CA(255, 196, 80, 22),
    .eraser = C(255, 130, 110), .samples = C(140, 220, 255),
    .surface = C(26, 70, 56), .surface_2 = C(36, 88, 70), .outline = C(64, 116, 96),
    .accent = C(244, 186, 64), .on_accent = C(28, 48, 38), .tint = CA(244, 186, 64, 44),
    .panel = C(26, 70, 56), .panel_border = C(64, 116, 96), .grip = C(120, 160, 140),
    .button = C(36, 88, 70), .button_selected = C(244, 186, 64), .button_text = C(236, 244, 238), .button_text_disabled = C(100, 140, 120),
    .danger = C(255, 126, 110), .field = C(20, 60, 48), .field_border = C(88, 140, 118), .field_placeholder = C(160, 192, 176),
    .slider_track = C(64, 116, 96), .slider_fill = C(244, 186, 64), .slider_thumb = C(244, 186, 64), .swatch_border = C(64, 116, 96),
};

#undef C
#undef CA

RDE_INTERNAL fude_theme fude_zoom_themes[FUDE_THEME_COUNT];
RDE_INTERNAL u32        fude_zoom_theme_texts[FUDE_THEME_COUNT];

void fude_zoom_themes_use(void) {
    fude_zoom_themes[FUDE_THEME_PAPER]  = *fude_theme_core(FUDE_THEME_PAPER);
    fude_zoom_themes[FUDE_THEME_WASHI]  = FUDE_ZOOM_THEME_KRAFT;
    fude_zoom_themes[FUDE_THEME_NIGHT]  = *fude_theme_core(FUDE_THEME_NIGHT);
    fude_zoom_themes[FUDE_THEME_MATCHA] = FUDE_ZOOM_THEME_BLUEPRINT;
    fude_zoom_themes[FUDE_THEME_SAKURA] = FUDE_ZOOM_THEME_MAT;
    fude_zoom_theme_texts[FUDE_THEME_PAPER]  = (u32)FUDE_TEXT_THEME_PAPER;
    fude_zoom_theme_texts[FUDE_THEME_WASHI]  = (u32)FUDE_TEXT_ZOOM_THEME_KRAFT;
    fude_zoom_theme_texts[FUDE_THEME_NIGHT]  = (u32)FUDE_TEXT_THEME_NIGHT;
    fude_zoom_theme_texts[FUDE_THEME_MATCHA] = (u32)FUDE_TEXT_ZOOM_THEME_BLUEPRINT;
    fude_zoom_theme_texts[FUDE_THEME_SAKURA] = (u32)FUDE_TEXT_ZOOM_THEME_MAT;
    fude_theme_use(fude_zoom_themes, fude_zoom_theme_texts);
}
