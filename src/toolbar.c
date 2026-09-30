#include "toolbar.h"
#include "userwords.h"
#include "theme.h"
#include "toolbar_kit.h"
#include "draw.h"
#include "icons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See toolbar.h.
// ===========================================================================

#define KANA_TOOLBAR_FONT_PATH   "assets/fonts/Roboto-Regular.ttf"
#define KANA_TOOLBAR_FONT_JP_PATH "assets/fonts/NotoSansJP-Regular.otf"
#define KANA_TOOLBAR_FONT_JP_GLYPHS 2048u   // distinct Japanese glyphs the font holds (see kana_toolbar_init)
#define KANA_TOOLBAR_FONT_ICONS_PATH      "assets/fonts/Phosphor-Regular.ttf"
#define KANA_TOOLBAR_FONT_ICONS_FILL_PATH "assets/fonts/Phosphor-Fill.ttf"
#define KANA_TOOLBAR_CHECK_FIELD (rde_vec_2F){ 360.0f, 44.0f }
#define KANA_TOOLBAR_FIELD_PX    14u      // a text field's text
#define KANA_TOOLBAR_TEXT_SCALE  (13.0f / 32.0f)   // 13 units of text from the 32 px font (Slug draws an em ~1.31 x the size: ~17 on screen)

// Geometry, UI canvas units (= window units: the canvas is CONSTANT_PIXEL).
#define KANA_TOOLBAR_BUTTON_W    44.0f
#define KANA_TOOLBAR_BUTTON_H    44.0f
#define KANA_TOOLBAR_TOOL_ICON_PX 17.0f   // ~22 on screen
#define KANA_TOOLBAR_DOT         22.0f    // Color's dot
#define KANA_TOOLBAR_GRIP        22.0f
#define KANA_TOOLBAR_GRIP_PX     15.0f    // its dots' size
#define KANA_TOOLBAR_SLIDER_LEN  110.0f
#define KANA_TOOLBAR_SLIDER_W    20.0f
#define KANA_TOOLBAR_SEPARATOR   9.0f     // along the bar: a hairline and room either side
#define KANA_TOOLBAR_SPACING     4.0f
#define KANA_TOOLBAR_PADDING     6.0f
#define KANA_TOOLBAR_RADIUS      16.0f
#define KANA_TOOLBAR_SCREEN_EDGE 8.0f     // the bar is kept at least this far inside the screen
#define KANA_TOOLBAR_MINIMIZED   48.0f    // the folded bar's length: just its grip, big enough for a finger
#define KANA_TOOLBAR_DOUBLE_TAP  0.35     // seconds between a double tap's two taps
#define KANA_TOOLBAR_DOUBLE_TAP_SLOP 32.0f   // and how far apart they can be

#define KANA_TOOLBAR_MENU_GAP    6.0f     // between the selection box and its menu
// A row of buttons (a screen's, the selection's): icons over labels.
#define KANA_TOOLBAR_ROW_BUTTON_H     56.0f
#define KANA_TOOLBAR_ROW_BUTTON_MIN_W 68.0f
#define KANA_TOOLBAR_ROW_BUTTON_MAX_W 136.0f
#define KANA_TOOLBAR_ROW_ICON_PX      17.0f
#define KANA_TOOLBAR_ROW_PADDING      6.0f
#define KANA_TOOLBAR_ROW_SPACING      4.0f
#define KANA_TOOLBAR_ROW_RADIUS       18.0f
#define KANA_TOOLBAR_CONTEXT_LIFT  56.0f  // the context menu sits this far above the finger
#define KANA_TOOLBAR_COPIED_TIME   1.2    // seconds Copy reads "Copied"

// Button order in each menu.
enum { KANA_SELECTION_CUT = 0, KANA_SELECTION_COPY, KANA_SELECTION_DUPLICATE, KANA_SELECTION_CHECK, KANA_SELECTION_DELETE, KANA_SELECTION_COUNT };
enum { KANA_CHECK_MENU_BACK = 0, KANA_CHECK_MENU_ORDER, KANA_CHECK_MENU_PRACTICE, KANA_CHECK_MENU_COUNT };
enum { KANA_CONTEXT_PASTE = 0, KANA_CONTEXT_SELECT_ALL, KANA_CONTEXT_COUNT };
enum { KANA_VIEWER_BACK = 0, KANA_VIEWER_PREV, KANA_VIEWER_REPLAY, KANA_VIEWER_NEXT, KANA_VIEWER_STUDY, KANA_VIEWER_PRACTICE, KANA_VIEWER_COUNT };
enum { KANA_CHART_MENU_HIRAGANA = 0, KANA_CHART_MENU_KATAKANA, KANA_CHART_MENU_SELECT, KANA_CHART_MENU_PRACTICE, KANA_CHART_MENU_CLOSE, KANA_CHART_MENU_COUNT };
enum { KANA_BROWSE_MENU_SELECT = 0, KANA_BROWSE_MENU_PRACTICE, KANA_BROWSE_MENU_CLOSE, KANA_BROWSE_MENU_COUNT };
enum { KANA_SELECT_MENU_ALL = 0, KANA_SELECT_MENU_NONE, KANA_SELECT_MENU_STUDY, KANA_SELECT_MENU_EXAM, KANA_SELECT_MENU_PRACTICE, KANA_SELECT_MENU_DONE, KANA_SELECT_MENU_COUNT };
enum { KANA_EXAM_SETUP_CLOSE = 0, KANA_EXAM_SETUP_NEXT, KANA_EXAM_SETUP_COUNT };
enum { KANA_EXAM_PREVIEW_BACK = 0, KANA_EXAM_PREVIEW_ALL, KANA_EXAM_PREVIEW_NONE, KANA_EXAM_PREVIEW_START, KANA_EXAM_PREVIEW_COUNT };
enum { KANA_EXAM_MENU_QUIT = 0, KANA_EXAM_MENU_UNDO, KANA_EXAM_MENU_CLEAR, KANA_EXAM_MENU_NEXT, KANA_EXAM_MENU_COUNT };
enum { KANA_EXAM_RESULTS_DONE = 0, KANA_EXAM_RESULTS_RETRY, KANA_EXAM_RESULTS_PRACTICE, KANA_EXAM_RESULTS_COUNT };
enum { KANA_STATS_MENU_CLOSE = 0, KANA_STATS_MENU_COUNT };
enum { KANA_VIEWER_ADD_DONE = 0, KANA_VIEWER_ADD_TYPE, KANA_VIEWER_ADD_COUNT };
#define KANA_TOOLBAR_WORD_CARD (rde_vec_2F){ 540.0f, 308.0f }
#define KANA_TOOLBAR_BACKDROP  (rde_color){ 0, 0, 0, 110 }
enum { KANA_PRACTICE_BACK = 0, KANA_PRACTICE_UNDO, KANA_PRACTICE_CLEAR, KANA_PRACTICE_SCORE, KANA_PRACTICE_FEWER, KANA_PRACTICE_MORE, KANA_PRACTICE_GUIDED, KANA_PRACTICE_COUNT };
enum { KANA_ALBUM_MENU_PRACTICE = KANA_ALBUM_SORT_COUNT, KANA_ALBUM_MENU_CLOSE, KANA_ALBUM_MENU_COUNT };   // the sorts first, in KANA_ALBUM_SORT_ order
enum { KANA_PRACTICE_SET_BACK = 0, KANA_PRACTICE_SET_UNDO, KANA_PRACTICE_SET_CLEAR, KANA_PRACTICE_SET_SCORE, KANA_PRACTICE_SET_GUIDED, KANA_PRACTICE_SET_NEXT, KANA_PRACTICE_SET_COUNT };
enum { KANA_PRACTICE_SUMMARY_AGAIN = 0, KANA_PRACTICE_SUMMARY_DONE, KANA_PRACTICE_SUMMARY_COUNT };

#define KANA_ALBUM_PRACTICE_MAX 10u   // the album's "Practice n": its weakest this many
enum { KANA_ALBUM_PAGE_BACK = 0, KANA_ALBUM_PAGE_PRACTICE, KANA_ALBUM_PAGE_COUNT };

// Browse's bar.
#define KANA_BROWSE_ROW_H     36.0f
#define KANA_BROWSE_FIELD_H   44.0f
#define KANA_BROWSE_SIDE_W    76.0f   // Draw, Clear
static const c8* const KANA_FILTER_LABELS[KANA_FILTER_COUNT] = { "All", "Hiragana", "Katakana", "Kanji", "N5", "N4", "N3", "N2", "N1", "Studying", "Known" };
static const c8* const KANA_SORT_LABELS[KANA_SORT_COUNT]     = { "Default", "Strokes", "On", "Kun", "Meaning" };

#define KANA_TOOLBAR_SWATCH      36.0f
#define KANA_TOOLBAR_CHOICE_W    72.0f    // the paper panel's choices: an icon over its name
#define KANA_TOOLBAR_CHOICE_H    60.0f
#define KANA_TOOLBAR_SWATCH_COLS 4u
#define KANA_TOOLBAR_PALETTE_GAP 8.0f

// Every colour below the palette is the theme's (theme.h), applied by
// kana_toolbar_apply_theme.
RDE_INTERNAL const rde_color KANA_TOOLBAR_PALETTE[KANA_TOOLBAR_PALETTE_COUNT] = {
    {   0,   0,   0,   0 },   // KANA_THEME_INK: the theme's ink, dark on light pages, light on dark ones
    { 128, 128, 136, 255 },   // pencil grey: readable on every page
    { 230,  72,  72, 255 },
    { 240, 150,  50, 255 },
    { 240, 210,  70, 255 },
    {  90, 190, 110, 255 },
    {  80, 140, 235, 255 },
    { 170, 110, 220, 255 },
};

// --- styles -------------------------------------------------------------------

// The fonts the looks and icons need (set by kana_toolbar_init): Phosphor's two
// weights, and the UI font for an icon that is a Japanese character (字, あ).
RDE_INTERNAL rde_font* kana_toolbar_icons_regular = NULL;
RDE_INTERNAL rde_font* kana_toolbar_icons_fill    = NULL;
RDE_INTERNAL rde_font* kana_toolbar_glyph_font    = NULL;
// An icon label's user data, telling it from the button's own label: a Phosphor
// icon (it turns Fill when its button is chosen) or a character.
RDE_INTERNAL const c8 kana_toolbar_tag_icon  = 1;
RDE_INTERNAL const c8 kana_toolbar_tag_glyph = 2;

// Each icon's left bearing (Phosphor units, 1024 an em): where its ink starts.
// A centred label lands a glyph its whole bearing right of centre: the text
// engine centres the line by its ink's right edge (half the bearing), then
// justifies the row by its ink span (the other half). kana_toolbar_icon moves
// it back. Measured
// from assets/fonts/Phosphor-Regular.ttf for the icons in icons.h (sorted by
// code point; Fill draws the same shapes filled). An icon not here is left as is.
static const struct { u32 codepoint; u16 bearing; } KANA_TOOLBAR_ICON_BEARINGS[] = {
    { 0xE036u, 128 },
    { 0xE038u,  64 },
    { 0xE058u, 128 },
    { 0xE06Cu, 128 },
    { 0xE094u, 128 },
    { 0xE0E6u,  64 },
    { 0xE0F8u,  96 },
    { 0xE10Au, 128 },
    { 0xE138u, 288 },
    { 0xE13Au, 352 },
    { 0xE150u,  96 },
    { 0xE156u,  96 },
    { 0xE182u, 128 },
    { 0xE184u,  96 },
    { 0xE18Au,  96 },
    { 0xE196u, 160 },
    { 0xE19Au,  96 },
    { 0xE1CAu, 128 },
    { 0xE1CCu, 128 },
    { 0xE1D6u,  64 },
    { 0xE1E6u, 160 },
    { 0xE1FCu, 192 },
    { 0xE1FEu, 192 },
    { 0xE21Eu,  96 },
    { 0xE220u,  32 },
    { 0xE224u,  32 },
    { 0xE230u, 160 },
    { 0xE236u, 160 },
    { 0xE242u, 160 },
    { 0xE244u, 160 },
    { 0xE24Au,  96 },
    { 0xE256u,  96 },
    { 0xE258u,  96 },
    { 0xE266u,  96 },
    { 0xE272u,  64 },
    { 0xE296u, 160 },
    { 0xE2CEu,  96 },
    { 0xE2D8u,  64 },
    { 0xE2DCu, 160 },
    { 0xE2F0u, 128 },
    { 0xE2F6u, 127 },
    { 0xE30Cu,  95 },
    { 0xE32Au, 128 },
    { 0xE34Cu, 128 },
    { 0xE39Cu,  96 },
    { 0xE3ACu, 128 },
    { 0xE3B4u, 128 },
    { 0xE3D0u, 256 },
    { 0xE3D4u, 128 },
    { 0xE3F6u,  96 },
    { 0xE422u,  96 },
    { 0xE432u,  96 },
    { 0xE444u, 160 },
    { 0xE45Eu, 128 },
    { 0xE466u,  96 },
    { 0xE46Au,  64 },
    { 0xE47Cu,  95 },
    { 0xE4A2u,  96 },
    { 0xE4A6u, 128 },
    { 0xE4ACu,  64 },
    { 0xE4F6u, 192 },
    { 0xE53Au,  32 },
    { 0xE5A2u, 128 },
    { 0xE606u,  64 },
    { 0xE626u,  96 },
    { 0xE62Cu,   0 },
    { 0xE67Eu,  32 },
    { 0xE69Au, 128 },
    { 0xE69Eu, 128 },
    { 0xE6C8u,  96 },
    { 0xE742u,  96 },
    { 0xE746u, 128 },
    { 0xE74Eu,  32 },
    { 0xE758u, 160 },
    { 0xE794u, 192 },
    { 0xE806u, 132 },
    { 0xEA38u, 160 },
    { 0xEADCu, 128 },
    { 0xEAE0u,  96 },
    { 0xEAE2u, 320 },
    { 0xEDC6u,  64 },
    { 0xEDF2u, 192 }
};

f32 kana_toolbar_icon_bearing(const c8* _glyph) {
    const u8* _u = (const u8*)_glyph;
    if((_u[0] & 0xF0u) != 0xE0u) {
        return 0.0f;   // not a three-byte character: not Phosphor's
    }
    const u32 _cp = ((u32)(_u[0] & 0x0Fu) << 12) | ((u32)(_u[1] & 0x3Fu) << 6) | (u32)(_u[2] & 0x3Fu);
    u32 _lo = 0, _hi = (u32)(sizeof(KANA_TOOLBAR_ICON_BEARINGS) / sizeof(KANA_TOOLBAR_ICON_BEARINGS[0]));
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(KANA_TOOLBAR_ICON_BEARINGS[_mid].codepoint < _cp) { _lo = _mid + 1u; }
        else                                                 { _hi = _mid; }
    }
    return _lo < sizeof(KANA_TOOLBAR_ICON_BEARINGS) / sizeof(KANA_TOOLBAR_ICON_BEARINGS[0]) && KANA_TOOLBAR_ICON_BEARINGS[_lo].codepoint == _cp
         ? (f32)KANA_TOOLBAR_ICON_BEARINGS[_lo].bearing / 1024.0f : 0.0f;
}

rde_ui_style kana_toolbar_style(rde_color _tint, f32 _radius) {
    rde_ui_style _s  = rde_ui_style_default();
    _s.tint          = _tint;
    _s.corner_radius = _radius;
    return _s;
}

rde_color kana_toolbar_shade(rde_color _c, i32 _delta) {
    const i32 _r = (i32)_c.r + _delta;
    const i32 _g = (i32)_c.g + _delta;
    const i32 _b = (i32)_c.b + _delta;
    return (rde_color){ (u8)(_r < 0 ? 0 : (_r > 255 ? 255 : _r)),
                        (u8)(_g < 0 ? 0 : (_g > 255 ? 255 : _g)),
                        (u8)(_b < 0 ? 0 : (_b > 255 ? 255 : _b)), _c.a };
}

RDE_INTERNAL b8 kana_toolbar_same_color(rde_color _a, rde_color _b) {
    return _a.r == _b.r && _a.g == _b.g && _a.b == _b.b && _a.a == _b.a;
}

// Pressed a little: darker on a light colour, lighter on a dark one.
RDE_INTERNAL rde_color kana_toolbar_press(rde_color _c, i32 _amount) {
    const f32 _luma = 0.299f * (f32)_c.r + 0.587f * (f32)_c.g + 0.114f * (f32)_c.b;
    return kana_toolbar_shade(_c, _luma > 128.0f ? -_amount : _amount);
}

// The button's icon label (kana_toolbar_icon), NULL when it has none.
RDE_INTERNAL rde_ui_label* kana_toolbar_icon_of(rde_ui_button* _button) {
    rde_ui_node* _node = rde_ui_button_as_node(_button);
    for(u32 _i = 0; _i < rde_ui_node_get_child_count(_node); _i++) {
        rde_ui_node* _child = rde_ui_node_get_child(_node, _i);
        const any    _tag   = _child->user_data;
        if(_tag == (any)&kana_toolbar_tag_icon || _tag == (any)&kana_toolbar_tag_glyph) {
            return (rde_ui_label*)_child;
        }
    }
    return NULL;
}

// What goes on the button's colour: on the accent (or Delete's red), white; on
// the accent's tint (chosen), the accent; on anything else, the text colour.
RDE_INTERNAL rde_color kana_toolbar_label_color(rde_ui_button* _button) {
    const kana_theme* _t  = kana_theme_active();
    const rde_color   _bg = _button->styles.styles[RDE_UI_STATE_NORMAL].tint;
    if(kana_toolbar_same_color(_bg, _t->accent) || kana_toolbar_same_color(_bg, _t->danger)) {
        return _t->on_accent;
    }
    if(kana_toolbar_same_color(_bg, _t->tint)) {
        return _t->accent;
    }
    return _t->button_text;
}

// The label and the icon in _color.
RDE_INTERNAL void kana_toolbar_paint(rde_ui_button* _button, rde_color _color) {
    if(_button->internal_label != NULL) {
        rde_ui_label_set_color(_button->internal_label, _color);
    }
    rde_ui_label* _icon = kana_toolbar_icon_of(_button);
    if(_icon != NULL) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// Normal / hovered / pressed / disabled from one base colour, with an optional
// border; the label and icon coloured to go on it. A clear base (a quiet
// button) shows the plain button's colour while pressed.
void kana_toolbar_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border) {
    const kana_theme* _t     = kana_theme_active();
    const b8          _clear = _base.a == 0;
    rde_ui_style      _s     = kana_toolbar_style(_base, 10.0f);
    _s.border_width  = _border_width;
    _s.border_color  = _border;
    rde_ui_button_set_style(_button, RDE_UI_STATE_NORMAL, _s);
    _s.tint = _clear ? _t->button : kana_toolbar_press(_base, 8);
    rde_ui_button_set_style(_button, RDE_UI_STATE_HOVERED, _s);
    _s.tint = _clear ? kana_toolbar_press(_t->button, 14) : kana_toolbar_press(_base, 22);
    rde_ui_button_set_style(_button, RDE_UI_STATE_PRESSED, _s);
    // Disabled: a coloured button goes plain (its label greys, kana_toolbar_set_enabled).
    const b8 _strong = kana_toolbar_same_color(_base, _t->accent) || kana_toolbar_same_color(_base, _t->danger) ||
                       kana_toolbar_same_color(_base, _t->tint);
    _s.tint = _strong ? _t->button : _base;
    rde_ui_button_set_style(_button, RDE_UI_STATE_DISABLED, _s);
    kana_toolbar_paint(_button, rde_ui_button_as_node(_button)->interactable ? kana_toolbar_label_color(_button) : _t->button_text_disabled);
}

// A Phosphor icon in its regular weight, or Fill (a chosen button's).
RDE_INTERNAL void kana_toolbar_icon_weight(rde_ui_button* _button, b8 _fill) {
    rde_ui_label* _icon = kana_toolbar_icon_of(_button);
    if(_icon != NULL && rde_ui_label_as_node(_icon)->user_data == (any)&kana_toolbar_tag_icon) {
        rde_font* _font = _fill && kana_toolbar_icons_fill != NULL ? kana_toolbar_icons_fill : kana_toolbar_icons_regular;
        if(_font != NULL) {
            rde_ui_label_set_font(_icon, _font);
        }
    }
}

void kana_toolbar_button_plain(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, kana_theme_active()->button, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_icon_weight(_button, false);
}

void kana_toolbar_button_quiet(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, (rde_color){ 0, 0, 0, 0 }, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_icon_weight(_button, false);
}

void kana_toolbar_button_selected(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, kana_theme_active()->tint, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_icon_weight(_button, true);
}

void kana_toolbar_button_primary(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, kana_theme_active()->accent, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_icon_weight(_button, false);
}

void kana_toolbar_button_danger(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, kana_theme_active()->danger, 0.0f, (rde_color){ 0, 0, 0, 0 });
    kana_toolbar_icon_weight(_button, false);
}

// Quiet, its label and icon in the danger colour (Delete in a row of quiet ones).
void kana_toolbar_button_danger_quiet(rde_ui_button* _button) {
    kana_toolbar_button_quiet(_button);
    if(rde_ui_button_as_node(_button)->interactable) {
        kana_toolbar_paint(_button, kana_theme_active()->danger);
    }
}

// A chip (Browse's filters and sorts): a pill, the accent when chosen.
void kana_toolbar_button_chip(rde_ui_button* _button, b8 _on) {
    if(_on) {
        kana_toolbar_button_primary(_button);
    } else {
        kana_toolbar_button_plain(_button);
    }
    kana_toolbar_button_round(_button, 999.0f);
}

// Every state's corners (the looks above are 10 units).
void kana_toolbar_button_round(rde_ui_button* _button, f32 _radius) {
    const RDE_UI_STATE_ _states[] = { RDE_UI_STATE_NORMAL, RDE_UI_STATE_HOVERED, RDE_UI_STATE_PRESSED, RDE_UI_STATE_DISABLED };
    for(u32 _i = 0; _i < sizeof(_states) / sizeof(_states[0]); _i++) {
        rde_ui_style _s  = _button->styles.styles[_states[_i]];
        _s.corner_radius = _radius;
        rde_ui_button_set_style(_button, _states[_i], _s);
    }
}

rde_color kana_toolbar_text_on(rde_color _background) {
    const f32 _luma = 0.299f * (f32)_background.r + 0.587f * (f32)_background.g + 0.114f * (f32)_background.b;
    return _luma > 140.0f ? (rde_color){ 20, 20, 24, 255 } : (rde_color){ 240, 240, 244, 255 };
}

// A panel (the bar, the palette, a menu, a card) in the theme's colours: the
// surface, a hairline border.
void kana_toolbar_style_panel(rde_ui_image* _panel, f32 _radius, f32 _border_width) {
    rde_ui_style _s = kana_toolbar_style(kana_theme_active()->panel, _radius);
    _s.border_width = _border_width;
    _s.border_color = kana_theme_active()->panel_border;
    rde_ui_image_set_style(_panel, RDE_UI_STATE_NORMAL, _s);
}

// --- creation helpers ----------------------------------------------------------

// Every child is pinned bottom-left to its parent with a centre pivot, so
// set_position places its CENTRE in parent-local units (bottom-left origin).
void kana_toolbar_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_anchor_preset(_node, RDE_UI_ANCHOR_PRESET_BOTTOM_LEFT);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_size(_node, _size);
    rde_ui_node_set_position(_node, _center);
}

// Pins _node to its parent's edges: its rect is the parent's anchor box
// (_anchor_min.._anchor_max, fractions) moved in by _inset_min / _inset_max.
RDE_INTERNAL void kana_toolbar_pin(rde_ui_node* _node, rde_vec_2F _anchor_min, rde_vec_2F _anchor_max, rde_vec_2F _inset_min, rde_vec_2F _inset_max) {
    rde_ui_node_set_anchors(_node, _anchor_min, _anchor_max);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_offsets(_node, _inset_min, (rde_vec_2F){ -_inset_max.x, -_inset_max.y });
}

rde_ui_button* kana_toolbar_button(kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click) {
    rde_ui_button* _button = rde_ui_button_create(_toolbar->font, NULL);
    rde_ui_button_set_text(_button, _text);
    kana_toolbar_button_plain(_button);

    // Fit the text, never truncate it: shrink a long word into the button.
    rde_ui_label_set_font_scale(_button->internal_label, KANA_TOOLBAR_TEXT_SCALE);
    rde_ui_label_set_auto_fit(_button->internal_label, true);
    rde_ui_label_set_auto_fit_min_scale(_button->internal_label, 0.3f);
    rde_ui_label_set_color(_button->internal_label, kana_theme_active()->button_text);

    rde_ui_button_set_on_click(_button, _on_click, _toolbar);
    rde_ui_node_add_child(_parent, rde_ui_button_as_node(_button));
    return _button;
}

// Gives _button an icon (icons.h; or a Japanese character, drawn in the UI
// font), _px tall, set out as _at says: alone, over its label (a row's
// buttons), or before it (a list's rows, left-aligned). Again on the same button
// changes the icon.
void kana_toolbar_icon(rde_ui_button* _button, const c8* _glyph, KANA_TOOLBAR_ICON_AT_ _at, f32 _px) {
    const b8      _phosphor = (u8)_glyph[0] == 0xEEu || (u8)_glyph[0] == 0xEFu;   // U+E000..U+F8FF: Phosphor's private use
    rde_ui_label* _icon     = kana_toolbar_icon_of(_button);
    if(_icon == NULL) {
        _icon = rde_ui_label_create(NULL);
        rde_ui_label_set_alignment(_icon, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node* _n = rde_ui_label_as_node(_icon);
        rde_ui_node_set_raycast_target(_n, false);
        rde_ui_node_set_interactable(_n, false);
        rde_ui_node_add_child(rde_ui_button_as_node(_button), _n);
    }
    rde_ui_node* _n = rde_ui_label_as_node(_icon);
    rde_ui_node_set_user_data(_n, (any)(_phosphor ? &kana_toolbar_tag_icon : &kana_toolbar_tag_glyph));
    rde_font* _font = _phosphor ? kana_toolbar_icons_regular : kana_toolbar_glyph_font;
    if(_font != NULL) {
        rde_ui_label_set_font(_icon, _font);
    }
    rde_ui_label_set_font_scale(_icon, _px / (f32)KANA_TOOLBAR_FONT_SIZE);
    rde_ui_label_set_text(_icon, _glyph);

    rde_ui_label* _text = _button->internal_label;
    // Back left by the icon's bearing (see KANA_TOOLBAR_ICON_BEARINGS).
    const f32 _back = _phosphor ? kana_toolbar_icon_bearing(_glyph) * _px * KANA_TOOLBAR_EM : 0.0f;
    if(_at == KANA_TOOLBAR_ICON_ONLY) {
        kana_toolbar_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, 0.0f }, (rde_vec_2F){ _back, 0.0f });
        if(_text != NULL) {
            rde_ui_node_set_active(rde_ui_label_as_node(_text), false);
        }
    } else if(_at == KANA_TOOLBAR_ICON_ABOVE) {
        // The icon in the top of the button, the label a line under it.
        kana_toolbar_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ -_back, 20.0f }, (rde_vec_2F){ _back, 3.0f });
        if(_text != NULL) {
            kana_toolbar_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 0.0f }, (rde_vec_2F){ 3.0f, 5.0f }, (rde_vec_2F){ 3.0f, -21.0f });
            rde_ui_label_set_font_scale(_text, KANA_TOOLBAR_CAPTION_PX / (f32)KANA_TOOLBAR_FONT_SIZE);
        }
    } else {
        // At the left, the label left-aligned after it.
        const f32 _em = _px * KANA_TOOLBAR_EM;
        kana_toolbar_pin(_n, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 1.0f }, (rde_vec_2F){ 10.0f - _back, 0.0f }, (rde_vec_2F){ -(10.0f + _em - _back), 0.0f });
        if(_text != NULL) {
            kana_toolbar_pin(rde_ui_label_as_node(_text), (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 20.0f + _em, 0.0f }, (rde_vec_2F){ 8.0f, 0.0f });
            rde_ui_label_set_alignment(_text, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        }
    }
    kana_toolbar_paint(_button, rde_ui_button_as_node(_button)->interactable ? kana_toolbar_label_color(_button) : kana_theme_active()->button_text_disabled);
}

// The icon alone in another colour (a list's row: the icon in the accent).
void kana_toolbar_icon_color(rde_ui_button* _button, rde_color _color) {
    rde_ui_label* _icon = kana_toolbar_icon_of(_button);
    if(_icon != NULL && rde_ui_button_as_node(_button)->interactable) {
        rde_ui_label_set_color(_icon, _color);
    }
}

// A text field in its box: a rounded frame in the field's colour with a
// hairline, which holds the field (the field draws no background of its own —
// the text editor's is a plain rectangle). Placed and shown through the box
// (kana_toolbar_field_node); the field fills it.
void kana_toolbar_field_box(rde_ui_node* _parent, rde_ui_text_editor* _field) {
    rde_ui_image* _frame = rde_ui_image_create(NULL);
    rde_ui_node*  _n     = rde_ui_image_as_node(_frame);
    rde_ui_node_set_raycast_target(_n, false);
    rde_ui_node_set_user_data(_n, (any)&kana_toolbar_tag_glyph);   // known again by kana_toolbar_field_node
    rde_ui_node_add_child(_parent, _n);
    rde_ui_node* _f = rde_ui_text_editor_as_node(_field);
    rde_ui_node_add_child(_n, _f);
    kana_toolbar_pin(_f, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 1.0f, 1.0f }, (rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ 0.0f, 0.0f });
}

rde_ui_node* kana_toolbar_field_node(rde_ui_text_editor* _field) {
    rde_ui_node* _f      = rde_ui_text_editor_as_node(_field);
    rde_ui_node* _parent = rde_ui_node_get_parent(_f);
    return _parent != NULL && _parent->user_data == (any)&kana_toolbar_tag_glyph ? _parent : _f;
}

// A field in the theme's colours: its box, its text, its caret.
void kana_toolbar_style_field(rde_ui_text_editor* _field) {
    const kana_theme* _t = kana_theme_active();
    rde_ui_text_editor_set_background(_field, false, _t->field);
    rde_ui_text_editor_set_placeholder_color(_field, _t->field_placeholder);
    rde_ui_text_editor_set_text_color(_field, _t->text);
    rde_ui_text_editor_set_caret_color(_field, _t->accent);
    rde_ui_node* _box = kana_toolbar_field_node(_field);
    if(_box != rde_ui_text_editor_as_node(_field)) {
        rde_ui_style _s = kana_toolbar_style(_t->field, 10.0f);
        _s.border_width = 1.0f;
        _s.border_color = _t->field_border;
        rde_ui_image_set_style((rde_ui_image*)_box, RDE_UI_STATE_NORMAL, _s);
    }
}

// --- state → widgets -----------------------------------------------------------

RDE_INTERNAL void kana_toolbar_refresh(kana_toolbar* _toolbar) {
    const struct { rde_ui_button* button; KANA_TOOL_ tool; } _tools[] = {
        { _toolbar->draw,       KANA_TOOL_DRAW  },
        { _toolbar->erase,      KANA_TOOL_ERASE },
        { _toolbar->lasso_tool, KANA_TOOL_LASSO },
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        if(_tools[_i].tool == _toolbar->tool) {
            kana_toolbar_button_selected(_tools[_i].button);
        } else {
            kana_toolbar_button_quiet(_tools[_i].button);
        }
    }

    // Color's dot IS the current colour (the theme's ink, when that is it),
    // ringed so white or the page's own colour still shows.
    const rde_color _ink = kana_theme_resolve(_toolbar->ink->color);
    rde_ui_style    _dot = kana_toolbar_style(_ink, KANA_TOOLBAR_DOT * 0.5f);
    _dot.border_width    = 2.0f;
    _dot.border_color    = kana_theme_active()->outline;
    rde_ui_image_set_style(_toolbar->color_dot, RDE_UI_STATE_NORMAL, _dot);
    kana_toolbar_button_quiet(_toolbar->color);
    if(_toolbar->palette_open) {
        kana_toolbar_button_selected(_toolbar->color);
    }

    // Page or Screen: what the brush's width keeps to.
    rde_ui_button_set_text(_toolbar->brush_scale, _toolbar->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? "Page" : "Screen");
    kana_toolbar_icon(_toolbar->brush_scale, _toolbar->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_ICON_PAGE : KANA_ICON_TABLET,
                      KANA_TOOLBAR_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);

    // The paper panel shows the page's own paper chosen: it follows the canvas
    // open. Paper shows it too.
    static const c8* const _paper_icons[KANA_PAPER_COUNT] = { KANA_ICON_PAPER_DOTS, KANA_ICON_PAPER_SQUARES, KANA_ICON_PAPER_LINES, KANA_ICON_PAPER_NONE };
    for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
        if((KANA_PAPER_)_i == _toolbar->view->page.paper) {
            kana_toolbar_button_selected(_toolbar->paper_choices[_i]);
        } else {
            kana_toolbar_button_quiet(_toolbar->paper_choices[_i]);
        }
    }
    const u32 _paper = (u32)_toolbar->view->page.paper < KANA_PAPER_COUNT ? (u32)_toolbar->view->page.paper : 0u;
    kana_toolbar_icon(_toolbar->paper, _paper_icons[_paper], KANA_TOOLBAR_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);
    if(_toolbar->paper_open) { kana_toolbar_button_selected(_toolbar->paper); } else { kana_toolbar_button_quiet(_toolbar->paper); }
    _toolbar->_paper_shown = _toolbar->view->page.paper;

    rde_ui_slider_set_value(_toolbar->size, _toolbar->ink->constant_radius);
}

void kana_toolbar_set_enabled(rde_ui_button* _button, b8 _enabled) {
    rde_ui_node_set_interactable(rde_ui_button_as_node(_button), _enabled);
    kana_toolbar_paint(_button, _enabled ? kana_toolbar_label_color(_button) : kana_theme_active()->button_text_disabled);
}

// Quiet again (a row's button, a tool), its label coloured by whether it can be pressed.
void kana_toolbar_restyle_quiet(rde_ui_button* _button) {
    kana_toolbar_button_quiet(_button);
    kana_toolbar_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

// A plain button again, its label coloured by whether it can be pressed.
void kana_toolbar_restyle_button(rde_ui_button* _button) {
    kana_toolbar_button_plain(_button);
    kana_toolbar_set_enabled(_button, rde_ui_button_as_node(_button)->interactable);
}

// Defined below.
void       kana_toolbar_set_palette_open(kana_toolbar* _toolbar, b8 _open);
RDE_INTERNAL void kana_toolbar_set_paper_open(kana_toolbar* _toolbar, b8 _open);
RDE_INTERNAL void kana_toolbar_show_guided(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_show_study(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_show_exam(kana_toolbar* _toolbar, b8 _examining, rde_vec_2F _center);
RDE_INTERNAL void kana_toolbar_layout_word_form(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_close_word_form(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_layout(kana_toolbar* _toolbar);
rde_vec_2F kana_toolbar_virtual_size(const kana_toolbar* _toolbar);
rde_vec_2F kana_toolbar_clamp(const kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size);

RDE_INTERNAL b8 kana_toolbar_same_vec(rde_vec_2F _a, rde_vec_2F _b) {
    return memcmp(&_a, &_b, sizeof(rde_vec_2F)) == 0;
}

// A menu: a panel under the root with a row of buttons, each an icon over its
// label, hidden until shown. A button is as wide as its label needs (a label
// ending in a count, "Practice 0", with room for three digits).
RDE_INTERNAL void kana_toolbar_menu_create(kana_toolbar* _toolbar, kana_toolbar_menu* _menu, rde_ui_node* _root,
                                           const c8* const* _labels, const c8* const* _icons, const rde_ui_event_callback* _callbacks, u32 _count) {
    _menu->count = _count < KANA_TOOLBAR_MENU_MAX ? _count : KANA_TOOLBAR_MENU_MAX;
    _menu->panel = rde_ui_image_create(NULL);
    kana_toolbar_style_panel(_menu->panel, KANA_TOOLBAR_ROW_RADIUS, 1.0f);

    rde_ui_node* _node = rde_ui_image_as_node(_menu->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    f32 _widths[KANA_TOOLBAR_MENU_MAX];
    f32 _total = 2.0f * KANA_TOOLBAR_ROW_PADDING + KANA_TOOLBAR_ROW_SPACING * (f32)(_menu->count - 1u);
    for(u32 _i = 0; _i < _menu->count; _i++) {
        c8          _measure[48];
        const usize _len = strlen(_labels[_i]);
        const b8    _count_at_end = _len >= 2 && _labels[_i][_len - 1] == '0' && _labels[_i][_len - 2] == ' ';
        snprintf(_measure, sizeof(_measure), _count_at_end ? "%s00" : "%s", _labels[_i]);
        const f32 _w = kana_draw_text_width(_toolbar->font, (f32)KANA_TOOLBAR_FONT_SIZE, _measure, KANA_TOOLBAR_CAPTION_PX) + 24.0f;
        _widths[_i]  = rde_math_clamp_f32(_w, KANA_TOOLBAR_ROW_BUTTON_MIN_W, KANA_TOOLBAR_ROW_BUTTON_MAX_W);
        _total      += _widths[_i];
    }
    _menu->size = (rde_vec_2F){ _total, KANA_TOOLBAR_ROW_BUTTON_H + 2.0f * KANA_TOOLBAR_ROW_PADDING };

    f32 _x = KANA_TOOLBAR_ROW_PADDING;
    for(u32 _i = 0; _i < _menu->count; _i++) {
        _menu->buttons[_i] = kana_toolbar_button(_toolbar, _node, _labels[_i], _callbacks[_i]);
        kana_toolbar_place(rde_ui_button_as_node(_menu->buttons[_i]), (rde_vec_2F){ _x + _widths[_i] * 0.5f, _menu->size.y * 0.5f },
                           (rde_vec_2F){ _widths[_i], KANA_TOOLBAR_ROW_BUTTON_H });
        kana_toolbar_icon(_menu->buttons[_i], _icons[_i], KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
        kana_toolbar_button_round(_menu->buttons[_i], 12.0f);
        _x += _widths[_i] + KANA_TOOLBAR_ROW_SPACING;
    }

    rde_ui_node_set_active(_node, false);
}

// Shows the menu centred at _center (UI units, clamped on screen), or hides it.
// Only touches the UI when something changed.
RDE_INTERNAL void kana_toolbar_menu_show(kana_toolbar* _toolbar, kana_toolbar_menu* _menu, b8 _show, rde_vec_2F _center) {
    if(_show) {
        _center = kana_toolbar_clamp(_toolbar, _center, _menu->size);
        if(!_menu->open || !kana_toolbar_same_vec(_center, _menu->center)) {
            _menu->center = _center;
            kana_toolbar_place(rde_ui_image_as_node(_menu->panel), _center, _menu->size);
        }
    }

    if(_show != _menu->open) {
        _menu->open = _show;
        rde_ui_node_set_active(rde_ui_image_as_node(_menu->panel), _show);
    }
}

// The selection menu floats just above the selection box — below it when there
// is no room above — and hides while the selection is being dragged.
RDE_INTERNAL void kana_toolbar_update_selection_menu(kana_toolbar* _toolbar) {
    rde_vec_2F _min;
    rde_vec_2F _max;
    kana_lasso_sync(_toolbar->lasso, _toolbar->ink);
    const b8 _show = !_toolbar->_viewer_shown && !kana_lasso_busy(_toolbar->lasso) && kana_lasso_bounds(_toolbar->lasso, _toolbar->ink, &_min, &_max);

    rde_vec_2F _center = _toolbar->selection_menu.center;

    if(_show) {
        // Canvas → Kana screen (centre origin) → UI canvas (bottom-left origin).
        const kana_view* _view   = &_toolbar->view->view;
        const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
        const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
        const rde_vec_2F _size   = _toolbar->selection_menu.size;
        const f32        _x      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x + _screen.x * 0.5f;
        const f32        _top    = _max.y * _view->zoom + _view->offset.y + _screen.y * 0.5f + KANA_LASSO_BOX_PAD;
        const f32        _bottom = _min.y * _view->zoom + _view->offset.y + _screen.y * 0.5f - KANA_LASSO_BOX_PAD;

        _center = (rde_vec_2F){ _x, _top + KANA_TOOLBAR_MENU_GAP + _size.y * 0.5f };
        if(_center.y + _size.y * 0.5f > _screen.y - (f32)_insets.y - KANA_TOOLBAR_SCREEN_EDGE) {
            _center.y = _bottom - KANA_TOOLBAR_MENU_GAP - _size.y * 0.5f;
        }
    }

    kana_toolbar_menu_show(_toolbar, &_toolbar->selection_menu, _show, _center);

    // "Copied" goes back to "Copy".
    if(_toolbar->copied_until > 0.0 && rde_engine_get_time_now() >= _toolbar->copied_until) {
        _toolbar->copied_until = 0.0;
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY], "Copy");
    }
}

RDE_INTERNAL void kana_toolbar_layout_browse(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_refresh_browse(kana_toolbar* _toolbar);

// The viewer's Study: the character's mark, Study / Studying / Known.
RDE_INTERNAL void kana_toolbar_show_study(kana_toolbar* _toolbar) {
    const kana_viewer* _viewer = _toolbar->viewer;
    if(rde_arr_length(&_viewer->list) == 0) {
        return;
    }
    kana_kanji_info _info;
    if(!kana_kanji_at(_viewer->db, ((const u32*)_viewer->list.memory)[_viewer->position], &_info)) {
        return;
    }
    const KANA_MARK_ _mark = kana_marks_get(_info.codepoint);
    if(_info.codepoint == _toolbar->_mark_shown_for && _mark == _toolbar->_mark_shown && kana_marks_revision() == _toolbar->_marks_seen) {
        return;
    }
    _toolbar->_mark_shown_for = _info.codepoint;
    _toolbar->_mark_shown     = _mark;
    _toolbar->_marks_seen     = kana_marks_revision();
    rde_ui_button* _study = _toolbar->viewer_menu.buttons[KANA_VIEWER_STUDY];
    rde_ui_button_set_text(_study, _mark == KANA_MARK_KNOWN ? "Known" : _mark == KANA_MARK_STUDYING ? "Studying" : "Study");
    kana_toolbar_icon(_study, _mark == KANA_MARK_KNOWN ? KANA_ICON_KNOWN : KANA_ICON_STAR, KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
    if(_mark == KANA_MARK_NONE) { kana_toolbar_button_quiet(_study); } else { kana_toolbar_button_selected(_study); }
    kana_toolbar_button_round(_study, 12.0f);
}

// The exam's row for its stage, and its labels (counts, Next / Finish).
RDE_INTERNAL void kana_toolbar_show_exam(kana_toolbar* _toolbar, b8 _examining, rde_vec_2F _center) {
    kana_exam* _exam  = _toolbar->exam;
    const b8   _setup = _examining && _exam->stage == KANA_EXAM_SETUP;
    const b8   _prev  = _examining && _exam->stage == KANA_EXAM_PREVIEW;
    const b8   _write = _examining && _exam->stage == KANA_EXAM_WRITING;
    const b8   _res   = _examining && _exam->stage == KANA_EXAM_RESULTS;
    kana_toolbar_menu_show(_toolbar, &_toolbar->exam_setup_menu, _setup, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->exam_preview_menu, _prev, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->exam_menu, _write, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->exam_results_menu, _res, _center);
    if(!_examining) {
        _toolbar->_exam_shown = UINT32_MAX;
        return;
    }

    u32 _wrong[KANA_EXAM_MAX];
    const u32 _planned  = _setup ? kana_exam_planned(_exam) : 0u;
    const u32 _included = _prev ? kana_exam_included(_exam) : 0u;
    const b8  _graded   = _res && kana_exam_graded(_exam);
    const u32 _nwrong   = _graded ? kana_exam_wrong(_exam, _wrong, KANA_EXAM_MAX) : 0u;
    const u32 _state    = (u32)_exam->stage | (_planned << 2) | (_included << 10) | ((u32)kana_exam_at_last(_exam) << 18) |
                          ((u32)_graded << 19) | (_nwrong << 20);
    if(_state == _toolbar->_exam_shown) {
        return;
    }
    _toolbar->_exam_shown = _state;
    c8 _label[32];
    snprintf(_label, sizeof(_label), "Next %u", _planned);
    rde_ui_button_set_text(_toolbar->exam_setup_menu.buttons[KANA_EXAM_SETUP_NEXT], _label);
    kana_toolbar_set_enabled(_toolbar->exam_setup_menu.buttons[KANA_EXAM_SETUP_NEXT], _planned > 0);
    snprintf(_label, sizeof(_label), "Start %u", _included);
    rde_ui_button_set_text(_toolbar->exam_preview_menu.buttons[KANA_EXAM_PREVIEW_START], _label);
    kana_toolbar_set_enabled(_toolbar->exam_preview_menu.buttons[KANA_EXAM_PREVIEW_START], _included > 0);
    rde_ui_button_set_text(_toolbar->exam_menu.buttons[KANA_EXAM_MENU_NEXT], kana_exam_at_last(_exam) ? "Finish" : "Next");
    kana_toolbar_icon(_toolbar->exam_menu.buttons[KANA_EXAM_MENU_NEXT], kana_exam_at_last(_exam) ? KANA_ICON_FINISH : KANA_ICON_ARROW_RIGHT,
                      KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
    kana_toolbar_set_enabled(_toolbar->exam_results_menu.buttons[KANA_EXAM_RESULTS_RETRY], _nwrong > 0);
    kana_toolbar_set_enabled(_toolbar->exam_results_menu.buttons[KANA_EXAM_RESULTS_PRACTICE], _nwrong > 0);
}

// The screens stack: Practice over the viewer, the viewer over Browse or the
// chart, and any of them over the page — the floating bar and its menus make way.
// Only the top screen's own row (or bar) shows.
RDE_INTERNAL void kana_toolbar_update_viewer(kana_toolbar* _toolbar) {
    const b8 _practicing = _toolbar->practice->open;
    const b8 _viewing    = _toolbar->viewer->open && !_practicing;
    const b8 _exam_open  = _toolbar->exam != NULL && _toolbar->exam->open;
    const b8 _examining  = _exam_open && !_toolbar->viewer->open && !_practicing;   // the viewer and Practice go over an exam's results
    const b8 _stats_open = _toolbar->stats != NULL && _toolbar->stats->open;
    const b8 _statsing   = _stats_open && !_toolbar->viewer->open && !_practicing;   // the viewer goes over a tapped character
    const b8 _under      = _toolbar->viewer->open || _practicing || _exam_open || _stats_open;   // something covers Browse / the chart / the album
    const b8 _browsing   = _toolbar->browse->open && !_under;
    const b8 _charting   = _toolbar->chart->open && !_under;
    const b8 _albuming   = _toolbar->album->open && !_under;
    const b8 _checking   = _toolbar->check->open && !_under;
    const b8 _full       = _practicing || _toolbar->viewer->open || _toolbar->browse->open || _toolbar->chart->open || _toolbar->album->open ||
                           _toolbar->check->open || _exam_open || _stats_open;

    if(_full != _toolbar->_viewer_shown) {
        _toolbar->_viewer_shown = _full;
        rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->panel), !_full);
        if(_full) {
            kana_toolbar_set_palette_open(_toolbar, false);
            kana_toolbar_set_paper_open(_toolbar, false);
            kana_toolbar_close_context_menu(_toolbar);
        }
    }

    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const rde_vec_2F _center = { _screen.x * 0.5f, (f32)_insets.w + KANA_TOOLBAR_SCREEN_EDGE + _toolbar->viewer_menu.size.y * 0.5f };

    // The safe area changed — at start iOS reports none (SDL has the whole
    // window until the view is laid out) and the real one arrives a frame or two
    // later: the bar is clamped again and Browse's bar laid out again.
    if(memcmp(&_insets, &_toolbar->_insets_seen, sizeof(rde_vec_4I)) != 0) {
        _toolbar->_insets_seen     = _insets;
        _toolbar->_browse_laid_out = (rde_vec_2F){ 0.0f, 0.0f };
        kana_toolbar_layout(_toolbar);
    }
    const b8 _adding = _viewing && _toolbar->viewer->adding;
    kana_toolbar_menu_show(_toolbar, &_toolbar->viewer_menu, _viewing && !_adding, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->viewer_add_menu, _adding, _center);
    if(_toolbar->word_open && !_adding) {
        kana_toolbar_close_word_form(_toolbar);   // the viewer went away under it
    }
    if(_toolbar->word_open) {
        kana_toolbar_layout_word_form(_toolbar);
    }
    if(_viewing) {
        kana_toolbar_show_study(_toolbar);
    }
    kana_toolbar_show_exam(_toolbar, _examining, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->stats_menu, _statsing, _center);
    // Select mode (select.h) ends when Browse and the chart are gone; while on, its
    // row takes their place.
    kana_selection* const _sel = _toolbar->selection;
    if(_sel != NULL && _sel->active && !_toolbar->browse->open && !_toolbar->chart->open) {
        _sel->active = false;
    }
    const b8 _selecting = _sel != NULL && _sel->active && (_browsing || _charting);
    kana_toolbar_menu_show(_toolbar, &_toolbar->chart_menu, _charting && !_selecting, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->browse_menu, _browsing && !_selecting, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->select_menu, _selecting, _center);
    if(_selecting && kana_selection_count(_sel) != _toolbar->_selected_shown) {
        _toolbar->_selected_shown = kana_selection_count(_sel);
        c8 _label[24];
        snprintf(_label, sizeof(_label), "Practice %u", _toolbar->_selected_shown);
        rde_ui_button_set_text(_toolbar->select_menu.buttons[KANA_SELECT_MENU_PRACTICE], _label);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_PRACTICE], _toolbar->_selected_shown > 0);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_NONE], _toolbar->_selected_shown > 0);
    }
    const b8 _summary = _practicing && _toolbar->practice->summary_open;
    const b8 _in_set  = _practicing && !_summary && kana_practice_in_set(_toolbar->practice);
    kana_toolbar_menu_show(_toolbar, &_toolbar->practice_menu, _practicing && !_summary && !_in_set, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->practice_set_menu, _in_set, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->practice_summary_menu, _summary, _center);

    // Next is Finish on a set's last character; Weakest again needs something weak.
    const b8 _finish = _in_set && kana_practice_at_last(_toolbar->practice);
    if(_in_set && _finish != _toolbar->_finish_shown) {
        _toolbar->_finish_shown = _finish;
        rde_ui_button_set_text(_toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_NEXT], _finish ? "Finish" : "Next");
        kana_toolbar_icon(_toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_NEXT], _finish ? KANA_ICON_FINISH : KANA_ICON_ARROW_RIGHT,
                          KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
    }
    // Guided: on or off; and Score only where there is something to score (not
    // steps 1 and 2, which are help).
    if(_toolbar->practice->guided != _toolbar->_guided_shown) {
        kana_toolbar_show_guided(_toolbar);
    }
    const b8 _can_score = !kana_practice_guiding(_toolbar->practice);
    if(_can_score != _toolbar->_can_score_shown) {
        _toolbar->_can_score_shown = _can_score;
        kana_toolbar_set_enabled(_toolbar->practice_menu.buttons[KANA_PRACTICE_SCORE], _can_score);
        kana_toolbar_set_enabled(_toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_SCORE], _can_score);
    }
    if(_summary) {
        const b8 _weak = kana_practice_weak_count(_toolbar->practice) > 0;
        if(_weak != rde_ui_button_as_node(_toolbar->practice_summary_menu.buttons[KANA_PRACTICE_SUMMARY_AGAIN])->interactable) {
            kana_toolbar_set_enabled(_toolbar->practice_summary_menu.buttons[KANA_PRACTICE_SUMMARY_AGAIN], _weak);
        }
    }

    // The album's "Practice n": its weakest, as many as there are up to 10.
    if(_albuming && !_toolbar->album->page_open) {
        const u32 _n = (u32)rde_arr_length(&_toolbar->album->entries) < KANA_ALBUM_PRACTICE_MAX ? (u32)rde_arr_length(&_toolbar->album->entries) : KANA_ALBUM_PRACTICE_MAX;
        if(_n != _toolbar->_album_practice_shown) {
            _toolbar->_album_practice_shown = _n;
            c8 _label[24];
            snprintf(_label, sizeof(_label), "Practice %u", _n);
            rde_ui_button_set_text(_toolbar->album_menu.buttons[KANA_ALBUM_MENU_PRACTICE], _n > 0 ? _label : "Practice");
            kana_toolbar_set_enabled(_toolbar->album_menu.buttons[KANA_ALBUM_MENU_PRACTICE], _n > 0);
        }
    }
    kana_toolbar_menu_show(_toolbar, &_toolbar->check_menu, _checking, _center);
    if(_checking != _toolbar->_check_field_shown) {
        _toolbar->_check_field_shown = _checking;
        rde_ui_node_set_active(kana_toolbar_field_node(_toolbar->check_field), _checking);
    }
    const rde_vec_4F _field_for = { _screen.x, _screen.y, (f32)_insets.y, (f32)_insets.z };
    if(_checking && memcmp(&_field_for, &_toolbar->_check_field_for, sizeof(rde_vec_4F)) != 0) {
        // Top right, on the row of Check's title (kana_check_render).
        _toolbar->_check_field_for = _field_for;
        const rde_vec_2F _size = KANA_TOOLBAR_CHECK_FIELD;
        const f32        _w    = fminf(_size.x, _screen.x - (f32)_insets.x - (f32)_insets.z - 180.0f);
        kana_toolbar_place(kana_toolbar_field_node(_toolbar->check_field),
                           (rde_vec_2F){ _screen.x - (f32)_insets.z - 16.0f - _w * 0.5f, _screen.y - (f32)_insets.y - 8.0f - _size.y * 0.5f },
                           (rde_vec_2F){ _w, _size.y });
    }
    kana_toolbar_menu_show(_toolbar, &_toolbar->album_menu, _albuming && !_toolbar->album->page_open, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->album_page_menu, _albuming && _toolbar->album->page_open, _center);

    if(_browsing != _toolbar->_browse_shown) {
        _toolbar->_browse_shown = _browsing;
        rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->browse_bar), _browsing);
        if(_browsing) {
            kana_toolbar_refresh_browse(_toolbar);
        }
    }
    if(_browsing && !kana_toolbar_same_vec(_screen, _toolbar->_browse_laid_out)) {
        kana_toolbar_layout_browse(_toolbar);   // first time, or the screen rotated
    }

    kana_side_update(_toolbar, _full);
}

void kana_toolbar_update(kana_toolbar* _toolbar) {
    if(_toolbar->ui == NULL) {
        return;
    }

    kana_toolbar_update_viewer(_toolbar);

    kana_toolbar_update_selection_menu(_toolbar);

    // Another canvas opened (or its file loaded): Squares shows that page's.
    if(_toolbar->view->page.paper != _toolbar->_paper_shown) {
        kana_toolbar_refresh(_toolbar);
    }

    const b8 _can_undo = kana_ink_can_undo(_toolbar->ink);
    const b8 _can_redo = kana_ink_can_redo(_toolbar->ink);

    if(!_toolbar->_history_shown || _can_undo != _toolbar->_can_undo_shown) {
        kana_toolbar_set_enabled(_toolbar->undo, _can_undo);
        _toolbar->_can_undo_shown = _can_undo;
    }

    if(!_toolbar->_history_shown || _can_redo != _toolbar->_can_redo_shown) {
        kana_toolbar_set_enabled(_toolbar->redo, _can_redo);
        _toolbar->_can_redo_shown = _can_redo;
    }

    _toolbar->_history_shown = true;
}

// --- layout ----------------------------------------------------------------------

rde_vec_2F kana_toolbar_virtual_size(const kana_toolbar* _toolbar) {
    const rde_vec_2I _w = rde_window_get_size(_toolbar->window);
    return (rde_vec_2F){ (f32)_w.x, (f32)_w.y };
}

// Keeps a rect of _size centred at _center inside the SAFE AREA — clear of the
// status bar, notch and home indicator. Done here rather than by the canvas: see
// the safe-area note in kana_toolbar_init.
rde_vec_2F kana_toolbar_clamp(const kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size) {
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const f32 _min_x = (f32)_insets.x + KANA_TOOLBAR_SCREEN_EDGE + _size.x * 0.5f;
    const f32 _max_x = _screen.x - (f32)_insets.z - KANA_TOOLBAR_SCREEN_EDGE - _size.x * 0.5f;
    const f32 _min_y = (f32)_insets.w + KANA_TOOLBAR_SCREEN_EDGE + _size.y * 0.5f;
    const f32 _max_y = _screen.y - (f32)_insets.y - KANA_TOOLBAR_SCREEN_EDGE - _size.y * 0.5f;
    return (rde_vec_2F){
        _min_x <= _max_x ? rde_math_clamp_f32(_center.x, _min_x, _max_x) : (_min_x + _max_x) * 0.5f,
        _min_y <= _max_y ? rde_math_clamp_f32(_center.y, _min_y, _max_y) : (_min_y + _max_y) * 0.5f
    };
}

// Where a pop-up of _size goes beside one of the bar's buttons (_button: its
// centre, UI units — kept within the bar, as the strip may have scrolled it out
// of sight): to the side of a vertical bar, above/below a horizontal one —
// whichever side has room.
RDE_INTERNAL rde_vec_2F kana_toolbar_beside(const kana_toolbar* _toolbar, rde_vec_2F _button_at, rde_vec_2F _size) {
    const rde_vec_2F _screen   = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_2F _panel_bl = { _toolbar->center.x - _toolbar->panel_size.x * 0.5f, _toolbar->center.y - _toolbar->panel_size.y * 0.5f };
    const rde_vec_2F _button   = { rde_math_clamp_f32(_button_at.x, _panel_bl.x, _panel_bl.x + _toolbar->panel_size.x),
                                   rde_math_clamp_f32(_button_at.y, _panel_bl.y, _panel_bl.y + _toolbar->panel_size.y) };
    rde_vec_2F       _center;

    if(_toolbar->vertical) {
        const f32 _right = _panel_bl.x + _toolbar->panel_size.x + KANA_TOOLBAR_PALETTE_GAP + _size.x * 0.5f;
        const f32 _left  = _panel_bl.x - KANA_TOOLBAR_PALETTE_GAP - _size.x * 0.5f;
        _center = (rde_vec_2F){ (_right + _size.x * 0.5f <= _screen.x) ? _right : _left, _button.y };
    } else {
        const f32 _below = _panel_bl.y - KANA_TOOLBAR_PALETTE_GAP - _size.y * 0.5f;
        const f32 _above = _panel_bl.y + _toolbar->panel_size.y + KANA_TOOLBAR_PALETTE_GAP + _size.y * 0.5f;
        _center = (rde_vec_2F){ _button.x, (_below - _size.y * 0.5f >= 0.0f) ? _below : _above };
    }

    return kana_toolbar_clamp(_toolbar, _center, _size);
}

RDE_INTERNAL void kana_toolbar_place_palette(kana_toolbar* _toolbar) {
    const f32        _cols   = (f32)KANA_TOOLBAR_SWATCH_COLS;
    const f32        _rows   = (f32)((KANA_TOOLBAR_PALETTE_COUNT + KANA_TOOLBAR_SWATCH_COLS - 1) / KANA_TOOLBAR_SWATCH_COLS);
    const rde_vec_2F _size   = { _cols * KANA_TOOLBAR_SWATCH + (_cols - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING,
                                 _rows * KANA_TOOLBAR_SWATCH + (_rows - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING };
    const rde_ui_node* _button = rde_ui_button_as_node(_toolbar->color);   // as laid out last frame
    const rde_vec_2F   _center = kana_toolbar_beside(_toolbar, (rde_vec_2F){ _button->rect.computed_position.x + _button->rect.computed_size.x * 0.5f,
                                                                           _button->rect.computed_position.y + _button->rect.computed_size.y * 0.5f }, _size);

    _toolbar->palette_center = _center;
    _toolbar->palette_size   = _size;
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->palette), _center, _size);

    for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
        const u32 _col = _i % KANA_TOOLBAR_SWATCH_COLS;
        const u32 _row = _i / KANA_TOOLBAR_SWATCH_COLS;
        const rde_vec_2F _local = {
            KANA_TOOLBAR_PADDING + (f32)_col * (KANA_TOOLBAR_SWATCH + KANA_TOOLBAR_SPACING) + KANA_TOOLBAR_SWATCH * 0.5f,
            _size.y - KANA_TOOLBAR_PADDING - (f32)_row * (KANA_TOOLBAR_SWATCH + KANA_TOOLBAR_SPACING) - KANA_TOOLBAR_SWATCH * 0.5f
        };
        kana_toolbar_place(rde_ui_button_as_node(_toolbar->swatches[_i]), _local, (rde_vec_2F){ KANA_TOOLBAR_SWATCH, KANA_TOOLBAR_SWATCH });
    }
}

// The paper panel: its choices in a row, beside the Paper button.
RDE_INTERNAL void kana_toolbar_place_paper(kana_toolbar* _toolbar) {
    const f32          _n      = (f32)KANA_PAPER_COUNT;
    const rde_vec_2F   _size   = { _n * KANA_TOOLBAR_CHOICE_W + (_n - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING,
                                   KANA_TOOLBAR_CHOICE_H + 2.0f * KANA_TOOLBAR_PADDING };
    const rde_ui_node* _button = rde_ui_button_as_node(_toolbar->paper);   // as laid out last frame
    const rde_vec_2F   _center = kana_toolbar_beside(_toolbar, (rde_vec_2F){ _button->rect.computed_position.x + _button->rect.computed_size.x * 0.5f,
                                                                           _button->rect.computed_position.y + _button->rect.computed_size.y * 0.5f }, _size);
    _toolbar->paper_center = _center;
    _toolbar->paper_size   = _size;
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->paper_panel), _center, _size);
    // In the order they read: dots, lines, squares, nothing.
    static const KANA_PAPER_ _order[KANA_PAPER_COUNT] = { KANA_PAPER_DOTS, KANA_PAPER_LINES, KANA_PAPER_SQUARES, KANA_PAPER_NONE };
    for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
        kana_toolbar_place(rde_ui_button_as_node(_toolbar->paper_choices[_order[_i]]),
                           (rde_vec_2F){ KANA_TOOLBAR_PADDING + (f32)_i * (KANA_TOOLBAR_CHOICE_W + KANA_TOOLBAR_SPACING) + KANA_TOOLBAR_CHOICE_W * 0.5f, _size.y * 0.5f },
                           (rde_vec_2F){ KANA_TOOLBAR_CHOICE_W, KANA_TOOLBAR_CHOICE_H });
    }
}

// Whichever of the bar's panels is open, placed against the bar where it is now.
RDE_INTERNAL void kana_toolbar_place_popups(kana_toolbar* _toolbar) {
    if(_toolbar->palette_open) {
        kana_toolbar_place_palette(_toolbar);
    }
    if(_toolbar->paper_open) {
        kana_toolbar_place_paper(_toolbar);
    }
}


// Lays the bar out along its axis — the grip, then the strip of tools, as long as
// they need or as the screen allows (then the strip scrolls); minimized, just the
// grip — and puts it at _toolbar->center, clamped on screen. Rotation is just
// this again.
RDE_INTERNAL void kana_toolbar_layout(kana_toolbar* _toolbar) {
    const b8 _v = _toolbar->vertical;

    // A separator is a hairline across the bar, with room either side of it.
    const rde_vec_2F _sep = _v ? (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W * 0.55f, KANA_TOOLBAR_SEPARATOR } : (rde_vec_2F){ KANA_TOOLBAR_SEPARATOR, KANA_TOOLBAR_BUTTON_H * 0.55f };
    struct { rde_ui_node* node; rde_vec_2F size; } _items[] = {
        { rde_ui_button_as_node(_toolbar->undo),        { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->redo),        { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_image_as_node(_toolbar->separators[0]), _sep },
        { rde_ui_button_as_node(_toolbar->draw),        { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->erase),       { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->lasso_tool),  { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->clear),       { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_image_as_node(_toolbar->separators[1]), _sep },
        { rde_ui_slider_as_node(_toolbar->size),        _v ? (rde_vec_2F){ KANA_TOOLBAR_SLIDER_W, KANA_TOOLBAR_SLIDER_LEN } : (rde_vec_2F){ KANA_TOOLBAR_SLIDER_LEN, KANA_TOOLBAR_SLIDER_W } },
        { rde_ui_image_as_node(_toolbar->separators[2]), _sep },
        { rde_ui_button_as_node(_toolbar->color),       { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->brush_scale), { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->paper),       { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_image_as_node(_toolbar->separators[3]), _sep },
        { rde_ui_button_as_node(_toolbar->rotate),      { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->reset_view),  { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
    };
    const u32 _count = sizeof(_items) / sizeof(_items[0]);

    // The slider runs along the bar; its thumb has to be wider than its track.
    rde_ui_slider_set_orientation(_toolbar->size, _v ? RDE_UI_ORIENTATION_VERTICAL : RDE_UI_ORIENTATION_HORIZONTAL);
    rde_ui_slider_set_thumb_size(_toolbar->size, _v ? (rde_vec_2F){ 28.0f, 14.0f } : (rde_vec_2F){ 14.0f, 28.0f });

    // The strip's content: every tool at its full size, padding at both ends.
    f32 _along  = 2.0f * KANA_TOOLBAR_PADDING + KANA_TOOLBAR_SPACING * (f32)(_count - 1);
    f32 _across = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        _along += _v ? _items[_i].size.y : _items[_i].size.x;
        const f32 _cross = _v ? _items[_i].size.x : _items[_i].size.y;
        _across = _cross > _across ? _cross : _across;
    }
    _across += 2.0f * KANA_TOOLBAR_PADDING;

    // The bar: the grip, then as much of the strip as the screen has room for.
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const f32        _room   = (_v ? _screen.y - (f32)(_insets.y + _insets.w) : _screen.x - (f32)(_insets.x + _insets.z)) - 2.0f * KANA_TOOLBAR_SCREEN_EDGE;
    const b8         _min    = _toolbar->minimized;
    const f32        _grip   = _min ? KANA_TOOLBAR_MINIMIZED : KANA_TOOLBAR_PADDING + KANA_TOOLBAR_GRIP;
    const f32        _strip  = _min ? 0.0f : fmaxf(KANA_TOOLBAR_BUTTON_H * 2.0f, fminf(_along, _room - _grip));
    _toolbar->panel_size = _v ? (rde_vec_2F){ _across, _grip + _strip } : (rde_vec_2F){ _grip + _strip, _across };
    rde_ui_node_set_active(rde_ui_scroll_area_as_node(_toolbar->strip), !_min);

    // The grip first: the top of a vertical bar, the left of a horizontal one —
    // all of that end, for a finger. The handle drawn in it sits against the
    // strip, or in the middle of a minimized bar.
    const rde_vec_2F _area = _v ? (rde_vec_2F){ _across, _grip } : (rde_vec_2F){ _grip, _across };
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->grip_area),
                       _v ? (rde_vec_2F){ _across * 0.5f, _strip + _grip * 0.5f } : (rde_vec_2F){ _grip * 0.5f, _across * 0.5f }, _area);
    const f32 _handle = _min ? _grip * 0.5f : KANA_TOOLBAR_PADDING + KANA_TOOLBAR_GRIP * 0.5f;   // from the bar's end
    const c8* _dots = _v ? KANA_ICON_GRIP_H : KANA_ICON_GRIP_V;   // the dots across the bar
    const f32 _back = kana_toolbar_icon_bearing(_dots) * KANA_TOOLBAR_GRIP_PX * KANA_TOOLBAR_EM;   // centred (see KANA_TOOLBAR_ICON_BEARINGS)
    rde_ui_label_set_text(_toolbar->grip, _dots);
    kana_toolbar_place(rde_ui_label_as_node(_toolbar->grip),
                       _v ? (rde_vec_2F){ _across * 0.5f - _back, _grip - _handle } : (rde_vec_2F){ _handle - _back, _across * 0.5f },
                       _v ? (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_GRIP } : (rde_vec_2F){ KANA_TOOLBAR_GRIP, KANA_TOOLBAR_BUTTON_H });

    // Then the strip, and the tools in its content (bottom-left origin; the
    // content's top is the strip's top when it has not scrolled).
    kana_toolbar_place(rde_ui_scroll_area_as_node(_toolbar->strip),
                       _v ? (rde_vec_2F){ _across * 0.5f, _strip * 0.5f } : (rde_vec_2F){ _grip + _strip * 0.5f, _across * 0.5f },
                       _v ? (rde_vec_2F){ _across, _strip } : (rde_vec_2F){ _strip, _across });
    rde_ui_scroll_area_set_content_size(_toolbar->strip, _v ? (rde_vec_2F){ _across, _along } : (rde_vec_2F){ _along, _across });

    // Vertical: top to bottom. Horizontal: left to right.
    f32 _cursor = _v ? _along - KANA_TOOLBAR_PADDING : KANA_TOOLBAR_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32  _len = _v ? _items[_i].size.y : _items[_i].size.x;
        rde_vec_2F _c;
        if(_v) {
            _c = (rde_vec_2F){ _across * 0.5f, _cursor - _len * 0.5f };
            _cursor -= _len + KANA_TOOLBAR_SPACING;
        } else {
            _c = (rde_vec_2F){ _cursor + _len * 0.5f, _across * 0.5f };
            _cursor += _len + KANA_TOOLBAR_SPACING;
        }
        // A separator takes its room along the bar but draws a hairline in it.
        b8 _separator = false;
        for(u32 _k = 0; _k < KANA_TOOLBAR_SEPARATORS; _k++) {
            _separator = _separator || _items[_i].node == rde_ui_image_as_node(_toolbar->separators[_k]);
        }
        const rde_vec_2F _hair = _v ? (rde_vec_2F){ _items[_i].size.x, 1.0f } : (rde_vec_2F){ 1.0f, _items[_i].size.y };
        kana_toolbar_place(_items[_i].node, _c, _separator ? _hair : _items[_i].size);
    }

    _toolbar->center = kana_toolbar_clamp(_toolbar, _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place_popups(_toolbar);
}

// Folds the bar down to its grip, or opens it again, with the grip's end staying
// where it is (under the finger that double tapped it) — then clamped on screen,
// so a bar opened near an edge moves back in.
RDE_INTERNAL void kana_toolbar_set_minimized(kana_toolbar* _toolbar, b8 _minimized) {
    const b8         _v    = _toolbar->vertical;
    const rde_vec_2F _c    = _toolbar->center;
    const f32        _edge = _v ? _c.y + _toolbar->panel_size.y * 0.5f : _c.x - _toolbar->panel_size.x * 0.5f;   // top / left

    _toolbar->minimized = _minimized;
    if(_minimized) {
        kana_toolbar_set_palette_open(_toolbar, false);
        kana_toolbar_set_paper_open(_toolbar, false);
    }
    kana_toolbar_layout(_toolbar);   // the new size

    const rde_vec_2F _size = _toolbar->panel_size;
    _toolbar->center = _v ? (rde_vec_2F){ _c.x, _edge - _size.y * 0.5f } : (rde_vec_2F){ _edge + _size.x * 0.5f, _c.y };
    kana_toolbar_layout(_toolbar);   // and there
}

void kana_toolbar_open_context_menu(kana_toolbar* _toolbar, rde_vec_2F _screen, rde_vec_2F _canvas) {
    if(_toolbar->ui == NULL) {
        return;
    }

    _toolbar->context_canvas = _canvas;
    kana_toolbar_set_enabled(_toolbar->context_menu.buttons[KANA_CONTEXT_PASTE], kana_lasso_can_paste(_toolbar->lasso));

    // Kana screen (centre origin) → UI canvas (bottom-left origin); above the
    // finger, or below it with no room above.
    const rde_vec_2F _ui     = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const rde_vec_2F _size   = _toolbar->context_menu.size;
    const rde_vec_2F _p      = { _screen.x + _ui.x * 0.5f, _screen.y + _ui.y * 0.5f };

    rde_vec_2F _center = { _p.x, _p.y + KANA_TOOLBAR_CONTEXT_LIFT + _size.y * 0.5f };
    if(_center.y + _size.y * 0.5f > _ui.y - (f32)_insets.y - KANA_TOOLBAR_SCREEN_EDGE) {
        _center.y = _p.y - KANA_TOOLBAR_CONTEXT_LIFT - _size.y * 0.5f;
    }

    kana_toolbar_menu_show(_toolbar, &_toolbar->context_menu, true, _center);
}

void kana_toolbar_close_context_menu(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL) {
        kana_toolbar_menu_show(_toolbar, &_toolbar->context_menu, false, _toolbar->context_menu.center);
    }
}

void kana_toolbar_set_placement(kana_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized) {
    _toolbar->vertical  = _vertical;
    _toolbar->center    = _center;
    _toolbar->minimized = _minimized;
    if(_minimized) {
        kana_toolbar_set_palette_open(_toolbar, false);
        kana_toolbar_set_paper_open(_toolbar, false);
    }
    kana_toolbar_layout(_toolbar);
}

// --- callbacks ---------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_undo(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_ink_undo(_toolbar->ink);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_redo(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_ink_redo(_toolbar->ink);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Leaving the Lasso tool drops the selection: it would otherwise sit there with
// no way to act on it.
RDE_INTERNAL void kana_toolbar_set_tool(kana_toolbar* _toolbar, KANA_TOOL_ _tool) {
    if(_tool != KANA_TOOL_LASSO) {
        kana_lasso_clear(_toolbar->lasso, _toolbar->ink);
    }
    _toolbar->tool = _tool;
    kana_toolbar_refresh(_toolbar);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_draw(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_set_tool((kana_toolbar*)_user_data, KANA_TOOL_DRAW);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_erase(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_set_tool((kana_toolbar*)_user_data, KANA_TOOL_ERASE);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_lasso(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_set_tool((kana_toolbar*)_user_data, KANA_TOOL_LASSO);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_delete_selection(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_lasso_delete(_toolbar->lasso, _toolbar->ink);   // one undoable edit
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_cut(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_lasso_cut(_toolbar->lasso, _toolbar->ink);      // copy, then one undoable delete
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_copy(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_lasso_copy(_toolbar->lasso, _toolbar->ink);
    // Nothing on the page changes, so say it worked.
    rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY], "Copied");
    _toolbar->copied_until = rde_engine_get_time_now() + KANA_TOOLBAR_COPIED_TIME;
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_duplicate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_lasso_duplicate(_toolbar->lasso, _toolbar->ink, _toolbar->view->view.zoom);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_paste(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_close_context_menu(_toolbar);
    // What was pasted comes in selected, ready to drag: that is the Lasso's job.
    kana_toolbar_set_tool(_toolbar, KANA_TOOL_LASSO);
    kana_lasso_paste(_toolbar->lasso, _toolbar->ink, _toolbar->context_canvas);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_select_all(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_close_context_menu(_toolbar);
    kana_toolbar_set_tool(_toolbar, KANA_TOOL_LASSO);
    kana_lasso_select_all(_toolbar->lasso, _toolbar->ink);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_ink_clear(_toolbar->ink);   // undoable: one Undo brings the page back
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

void kana_toolbar_set_palette_open(kana_toolbar* _toolbar, b8 _open) {
    if(_open) {
        kana_toolbar_set_paper_open(_toolbar, false);   // one panel at a time
    }
    _toolbar->palette_open = _open;
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->palette), _open);
    if(_open) {
        kana_toolbar_place_palette(_toolbar);
    }
    kana_toolbar_refresh(_toolbar);   // Color shows it open
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_color(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_set_palette_open(_toolbar, !_toolbar->palette_open);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}



RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_swatch(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_swatch_ref* _ref = (const kana_toolbar_swatch_ref*)_user_data;
    _ref->toolbar->ink->color = KANA_TOOLBAR_PALETTE[_ref->index];
    // Picking a colour means wanting to write with it.
    kana_toolbar_set_palette_open(_ref->toolbar, false);
    kana_toolbar_set_tool(_ref->toolbar, KANA_TOOL_DRAW);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL void kana_toolbar_set_paper_open(kana_toolbar* _toolbar, b8 _open) {
    if(_open && _toolbar->palette_open) {
        kana_toolbar_set_palette_open(_toolbar, false);   // one panel at a time
    }
    _toolbar->paper_open = _open;
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->paper_panel), _open);
    if(_open) {
        kana_toolbar_place_paper(_toolbar);
    }
    kana_toolbar_refresh(_toolbar);   // Paper shows it open
}

void kana_toolbar_open_paper(kana_toolbar* _toolbar) {
    kana_toolbar_set_paper_open(_toolbar, true);
}

// Paper opens (or closes) its panel.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_paper(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_set_paper_open(_toolbar, !_toolbar->paper_open);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A paper chosen: the page's, saved with it (the size of lines and squares is a
// setting: Settings › Lines & squares).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_paper_choice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_swatch_ref* _ref = (const kana_toolbar_swatch_ref*)_user_data;
    _ref->toolbar->view->page.paper = (KANA_PAPER_)_ref->index;
    kana_toolbar_set_paper_open(_ref->toolbar, false);
    kana_toolbar_refresh(_ref->toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_brush_scale(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->ink->brush_scale = _toolbar->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    kana_toolbar_refresh(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_rotate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->vertical = !_toolbar->vertical;
    kana_toolbar_layout(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_reset_view(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_canvas_reset_view(((kana_toolbar*)_user_data)->view);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}




// --- the album's rows ----------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_sort(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_chip_ref* _ref = (const kana_toolbar_chip_ref*)_user_data;
    kana_toolbar* _toolbar = _ref->toolbar;
    kana_album_set_sort(_toolbar->album, (KANA_ALBUM_SORT_)_ref->index);
    for(u32 _i = 0; _i < KANA_ALBUM_SORT_COUNT; _i++) {
        if(_i == _ref->index) { kana_toolbar_button_selected(_toolbar->album_menu.buttons[_i]); }
        else                  { kana_toolbar_button_quiet(_toolbar->album_menu.buttons[_i]); }
        kana_toolbar_button_round(_toolbar->album_menu.buttons[_i], 12.0f);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_album_close(_toolbar->album);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_album_close_page(_toolbar->album);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Practice on top of the page; its Back comes back here, to the new session.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->album->page_open) {
        kana_practice_open(_toolbar->practice, _toolbar->album->page_record);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- Check -------------------------------------------------------------------------

// The lasso's selection, read and checked (a character, or several).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_check(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_lasso_sync(_toolbar->lasso, _toolbar->ink);
    if(kana_check_open(_toolbar->check, _toolbar->ink, (const u32*)_toolbar->lasso->selected.memory, (u32)rde_arr_length(&_toolbar->lasso->selected))) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_toolbar->check_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_toolbar->check_field, 0, _bytes);   // a new selection: nothing meant yet
        }
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Return in "I meant…": the selection read as that (empty: freely again).
RDE_INTERNAL void kana_toolbar_on_check_meant(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    c8* _text = rde_ui_text_editor_get_text(_toolbar->check_field, 0, rde_ui_text_editor_get_byte_count(_toolbar->check_field));
    kana_check_read_as(_toolbar->check, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_toolbar->check_field, _text);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_check_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_check_close(_toolbar->check);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The characters read writing themselves: the viewer over Check, on the one
// looked at (Prev / Next go through the rest).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_check_order(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    const u32     _n       = kana_check_count(_toolbar->check);
    u32*          _records = _n > 0 ? (u32*)rde_malloc(sizeof(u32) * _n) : NULL;
    u32           _record  = 0;
    if(_records != NULL && kana_check_chosen_record(_toolbar->check, &_record)) {
        // Every character with a reading, in order; the viewer starts on the selected one.
        u32 _count = 0, _at = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            kana_check_char* _c = &((kana_check_char*)_toolbar->check->chars.memory)[_i];
            if(_c->chosen >= 0) {
                if(_i == _toolbar->check->selected) {
                    _at = _count;
                }
                _records[_count++] = _c->candidates[_c->chosen].record;
            }
        }
        kana_viewer_show(_toolbar->viewer, _records, _count, _at);
    }
    rde_free(_records);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Practice what was read: one character alone, several as a set (each once).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_check_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    u32           _records[KANA_PRACTICE_SET_MAX];
    const u32     _n = kana_check_records(_toolbar->check, _records, KANA_PRACTICE_SET_MAX, true);
    if(_n == 1) {
        kana_practice_open(_toolbar->practice, _records[0]);
    } else if(_n > 1) {
        kana_practice_open_set(_toolbar->practice, _records, _n);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- the chart's and Practice's rows ------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_chart_hiragana(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_chart_jump(((kana_toolbar*)_user_data)->chart, KANA_CHART_HIRAGANA);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_chart_katakana(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_chart_jump(((kana_toolbar*)_user_data)->chart, KANA_CHART_KATAKANA);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_chart_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_chart_close(_toolbar->chart);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Study: none → Studying → Known → none, for the character shown.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_study(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar*      _toolbar = (kana_toolbar*)_user_data;
    const kana_viewer* _viewer  = _toolbar->viewer;
    kana_kanji_info    _ch;
    if(rde_arr_length(&_viewer->list) > 0 && kana_kanji_at(_viewer->db, ((const u32*)_viewer->list.memory)[_viewer->position], &_ch)) {
        const KANA_MARK_ _mark = kana_marks_get(_ch.codepoint);
        kana_marks_set(_ch.codepoint, _mark == KANA_MARK_NONE ? KANA_MARK_STUDYING : _mark == KANA_MARK_STUDYING ? KANA_MARK_KNOWN : KANA_MARK_NONE);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- the viewer's Add: a word typed in ---------------------------------------------------

RDE_INTERNAL void kana_toolbar_layout_word_form(kana_toolbar* _toolbar) {
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    if(_screen.x == _toolbar->_word_laid_out.x && _screen.y == _toolbar->_word_laid_out.y) {
        return;
    }
    _toolbar->_word_laid_out = _screen;
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const rde_vec_2F _card   = { fminf(KANA_TOOLBAR_WORD_CARD.x, _screen.x - 32.0f), KANA_TOOLBAR_WORD_CARD.y };
    // High on the screen: the keyboard comes up under it.
    const rde_vec_2F _center = { _screen.x * 0.5f, _screen.y - (f32)_insets.y - 40.0f - _card.y * 0.5f };
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->word_backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->word_card), _center, _card);
    const f32 _m = 22.0f;
    kana_toolbar_place(rde_ui_label_as_node(_toolbar->word_title), (rde_vec_2F){ _card.x * 0.5f, _card.y - _m - 14.0f }, (rde_vec_2F){ _card.x - 2.0f * _m, 32.0f });
    for(u32 _i = 0; _i < 3u; _i++) {
        kana_toolbar_place(kana_toolbar_field_node(_toolbar->word_fields[_i]), (rde_vec_2F){ _card.x * 0.5f, _card.y - _m - 62.0f - (f32)_i * 52.0f },
                           (rde_vec_2F){ _card.x - 2.0f * _m, 44.0f });
    }
    kana_toolbar_place(rde_ui_label_as_node(_toolbar->word_error), (rde_vec_2F){ _card.x * 0.5f, _m + 58.0f }, (rde_vec_2F){ _card.x - 2.0f * _m, 22.0f });
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->word_add), (rde_vec_2F){ _card.x - _m - 50.0f, _m + 20.0f }, (rde_vec_2F){ 100.0f, 40.0f });
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->word_cancel), (rde_vec_2F){ _card.x - _m - 100.0f - 8.0f - 50.0f, _m + 20.0f }, (rde_vec_2F){ 100.0f, 40.0f });
}

RDE_INTERNAL void kana_toolbar_close_word_form(kana_toolbar* _toolbar) {
    _toolbar->word_open = false;
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->word_backdrop), false);
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->word_card), false);
}

RDE_INTERNAL void kana_toolbar_open_word_form(kana_toolbar* _toolbar) {
    const u32 _cp = kana_viewer_codepoint(_toolbar->viewer);
    if(_cp == 0) {
        return;
    }
    _toolbar->word_kanji = _cp;
    c8 _kanji[8];
    kana_kanji_utf8(_cp, _kanji);
    c8 _text[96];
    snprintf(_text, sizeof(_text), "Your word with %s", _kanji);
    rde_ui_label_set_text(_toolbar->word_title, _text);
    snprintf(_text, sizeof(_text), "Word, with %s in it", _kanji);
    rde_ui_text_editor_set_placeholder(_toolbar->word_fields[0], _text);
    for(u32 _i = 0; _i < 3u; _i++) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_toolbar->word_fields[_i]);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_toolbar->word_fields[_i], 0, _bytes);
        }
    }
    rde_ui_label_set_text(_toolbar->word_error, "");
    _toolbar->word_open      = true;
    _toolbar->_word_laid_out = (rde_vec_2F){ 0.0f, 0.0f };
    kana_toolbar_layout_word_form(_toolbar);
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->word_backdrop), true);
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->word_card), true);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_toolbar->word_fields[0]));   // the keyboard comes up
}

// A field's text, trimmed of spaces at both ends, into _out.
RDE_INTERNAL void kana_toolbar_word_field(kana_toolbar* _toolbar, u32 _i, c8* _out, usize _size) {
    c8*       _text = rde_ui_text_editor_get_text(_toolbar->word_fields[_i], 0, rde_ui_text_editor_get_byte_count(_toolbar->word_fields[_i]));
    const c8* _s    = _text != NULL ? _text : "";
    while(*_s == ' ') {
        _s++;
    }
    snprintf(_out, _size, "%s", _s);
    usize _n = strlen(_out);
    while(_n > 0 && _out[_n - 1] == ' ') {
        _out[--_n] = 0;
    }
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_toolbar->word_fields[_i], _text);
    }
}

// Add: the word, when it has the kanji and a reading (romaji becomes hiragana).
RDE_INTERNAL void kana_toolbar_word_submit(kana_toolbar* _toolbar) {
    c8 _written[KANA_USERWORD_WRITTEN], _reading[KANA_USERWORD_READING], _meaning[KANA_USERWORD_MEANING];
    kana_toolbar_word_field(_toolbar, 0, _written, sizeof(_written));
    kana_toolbar_word_field(_toolbar, 1, _reading, sizeof(_reading));
    kana_toolbar_word_field(_toolbar, 2, _meaning, sizeof(_meaning));

    b8        _has = false;
    const c8* _p   = _written;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0 && !_has; _cp = kana_kanji_utf8_next(&_p)) {
        _has = _cp == _toolbar->word_kanji;
    }
    c8 _kanji[8];
    kana_kanji_utf8(_toolbar->word_kanji, _kanji);
    c8 _error[96] = "";
    b8 _ascii = _reading[0] != 0;
    for(const c8* _c = _reading; *_c != 0; _c++) {
        _ascii = _ascii && (u8)*_c < 0x80u;
    }
    if(!_has) {
        snprintf(_error, sizeof(_error), "The word has to have %s in it", _kanji);
    } else if(_reading[0] == 0) {
        snprintf(_error, sizeof(_error), "Its reading, in kana or romaji");
    } else if(_ascii) {
        c8 _kana[KANA_USERWORD_READING];
        if(kana_romaji_to_hiragana(_reading, _kana, sizeof(_kana))) {
            snprintf(_reading, sizeof(_reading), "%s", _kana);
        } else {
            snprintf(_error, sizeof(_error), "That reading is not romaji Kana knows");
        }
    }
    if(_error[0] != 0) {
        rde_ui_label_set_text(_toolbar->word_error, _error);
        return;
    }
    kana_userwords_add(_toolbar->word_kanji, _written, _reading, _meaning);
    kana_toolbar_close_word_form(_toolbar);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_word_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_word_submit((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_word_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_close_word_form((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in a field: on to the next, or Add from the last.
RDE_INTERNAL void kana_toolbar_on_word_field(rde_ui_node* _node, any _user_data) {
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    for(u32 _i = 0; _i < 2u; _i++) {
        if(_node == rde_ui_text_editor_as_node(_toolbar->word_fields[_i])) {
            rde_ui_node_focus(rde_ui_text_editor_as_node(_toolbar->word_fields[_i + 1u]));
            return;
        }
    }
    kana_toolbar_word_submit(_toolbar);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_add_done(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_viewer_set_adding(_toolbar->viewer, false);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_add_type(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_open_word_form((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- exams ----------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_stats_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_stats_close(_toolbar->stats);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_close(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_next_stage(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_preview(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_back(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_all(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_tick_all(_toolbar->exam, true);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_none(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_tick_all(_toolbar->exam, false);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_start(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_start(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_undo(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_exam_undo(((kana_toolbar*)_user_data)->exam);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_exam_clear(((kana_toolbar*)_user_data)->exam);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_next(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_next(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_retry(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_exam_retry_wrong(_toolbar->exam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
// The wrong ones as a practice set, over the results.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_exam_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    u32       _wrong[KANA_EXAM_MAX];
    const u32 _n = kana_exam_wrong(_toolbar->exam, _wrong, KANA_EXAM_MAX);
    if(_n > 0) {
        kana_practice_open_set(_toolbar->practice, _wrong, _n);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar*      _toolbar = (kana_toolbar*)_user_data;
    const kana_viewer* _viewer  = _toolbar->viewer;
    if(rde_arr_length(&_viewer->list) > 0) {
        kana_practice_open(_toolbar->practice, ((const u32*)_viewer->list.memory)[_viewer->position]);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_practice_close(_toolbar->practice);
    kana_viewer_replay(_toolbar->viewer);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_undo(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_practice_undo(((kana_toolbar*)_user_data)->practice);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_practice_clear(((kana_toolbar*)_user_data)->practice);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_score(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_practice_score(((kana_toolbar*)_user_data)->practice);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_fewer(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_practice* _practice = ((kana_toolbar*)_user_data)->practice;
    kana_practice_set_squares(_practice, _practice->squares > 1u ? _practice->squares - 1u : 1u);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_more(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_practice* _practice = ((kana_toolbar*)_user_data)->practice;
    kana_practice_set_squares(_practice, _practice->squares + 1u);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Guided: its buttons show it on (both rows), and the squares' count means nothing then.
RDE_INTERNAL void kana_toolbar_show_guided(kana_toolbar* _toolbar) {
    const b8       _on        = _toolbar->practice->guided;
    rde_ui_button* _toggles[] = { _toolbar->practice_menu.buttons[KANA_PRACTICE_GUIDED], _toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_GUIDED] };
    for(u32 _i = 0; _i < sizeof(_toggles) / sizeof(_toggles[0]); _i++) {
        if(_on) {
            kana_toolbar_button_selected(_toggles[_i]);
        } else {
            kana_toolbar_button_quiet(_toggles[_i]);
        }
        kana_toolbar_button_round(_toggles[_i], 12.0f);
    }
    kana_toolbar_set_enabled(_toolbar->practice_menu.buttons[KANA_PRACTICE_FEWER], !_on);
    kana_toolbar_set_enabled(_toolbar->practice_menu.buttons[KANA_PRACTICE_MORE], !_on);
    _toolbar->_guided_shown = _on;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_guided(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_practice_set_guided(_toolbar->practice, !_toolbar->practice->guided);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- sets ------------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_next(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_practice_next(_toolbar->practice);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_again(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_practice_weakest_again(_toolbar->practice);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Done (the summary) and Back (a set): back to where the set began.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_practice_done(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_practice_close(_toolbar->practice);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Browse's list as a set: what is filtered, sorted and searched, in that order.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_browse_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(kana_browse_count(_toolbar->browse) > 0) {
        kana_practice_open_set(_toolbar->practice, kana_browse_list(_toolbar->browse), kana_browse_count(_toolbar->browse));
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The chart's section in view, in chart order.
// --- Select mode (Browse and the chart) -----------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_select_mode(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection != NULL) {
        _toolbar->selection->active = true;
        _toolbar->_selected_shown   = UINT32_MAX;   // the row's labels, from what is ticked
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// All: Browse's list as it is filtered, sorted and searched — or, in the chart,
// the section in view.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_all(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection == NULL) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    if(_toolbar->chart->open) {
        u32 _first = 0;
        u32 _count = 0;
        kana_chart_section_range(_toolbar->chart, kana_chart_section_in_view(_toolbar->chart), &_first, &_count);
        kana_selection_add(_toolbar->selection, &kana_chart_list(_toolbar->chart)[_first], _count);
    } else if(_toolbar->browse->open) {
        kana_selection_add(_toolbar->selection, kana_browse_list(_toolbar->browse), kana_browse_count(_toolbar->browse));
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_none(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection != NULL) {
        kana_selection_clear(_toolbar->selection);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The ticked, as a set, in the order ticked (Practice keeps the first 100).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection != NULL && kana_selection_count(_toolbar->selection) > 0) {
        kana_practice_open_set(_toolbar->practice, kana_selection_records(_toolbar->selection), kana_selection_count(_toolbar->selection));
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Study: the ticked marked Studying — or, when every one of them already is, no
// longer marked.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_study(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection == NULL || kana_selection_count(_toolbar->selection) == 0) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    static u32 _cps[16384];
    const u32  _n   = kana_selection_count(_toolbar->selection) < 16384u ? kana_selection_count(_toolbar->selection) : 16384u;
    b8         _all = true;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_kanji_info _info;
        kana_kanji_at(_toolbar->browse->db, kana_selection_records(_toolbar->selection)[_i], &_info);
        _cps[_i] = _info.codepoint;
        _all     = _all && kana_marks_get(_info.codepoint) == KANA_MARK_STUDYING;
    }
    kana_marks_set_many(_cps, _n, _all ? KANA_MARK_NONE : KANA_MARK_STUDYING);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Exam: the ticked, straight to the exam's preview.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_exam(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->exam != NULL && _toolbar->selection != NULL && kana_selection_count(_toolbar->selection) > 0) {
        kana_exam_open_with(_toolbar->exam, kana_selection_records(_toolbar->selection), kana_selection_count(_toolbar->selection));
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Done: taps open characters again; the ticks stay for next time.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_done(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection != NULL) {
        _toolbar->selection->active = false;
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_chart_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    u32 _first = 0;
    u32 _count = 0;
    kana_chart_section_range(_toolbar->chart, kana_chart_section_in_view(_toolbar->chart), &_first, &_count);
    if(_count > 0) {
        kana_practice_open_set(_toolbar->practice, &kana_chart_list(_toolbar->chart)[_first], _count);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The album's weakest.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_practice_weakest(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    u32       _records[KANA_ALBUM_PRACTICE_MAX];
    const u32 _n = kana_album_weakest(_toolbar->album, _records, KANA_ALBUM_PRACTICE_MAX);
    if(_n > 0) {
        kana_practice_open_set(_toolbar->practice, _records, _n);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- Browse's bar ------------------------------------------------------------------

RDE_INTERNAL void kana_toolbar_refresh_browse(kana_toolbar* _toolbar) {
    for(u32 _i = 0; _i < KANA_FILTER_COUNT; _i++) {
        kana_toolbar_button_chip(_toolbar->filter_chips[_i], (u32)_toolbar->browse->filter == _i);
    }
    for(u32 _i = 0; _i < KANA_SORT_COUNT; _i++) {
        kana_toolbar_button_chip(_toolbar->sort_chips[_i], (u32)_toolbar->browse->sort == _i);
    }
    if(_toolbar->browse->drawing) { kana_toolbar_button_selected(_toolbar->draw_toggle); }
    else                          { kana_toolbar_button_plain(_toolbar->draw_toggle); }
    if(_toolbar->browse->picking) { kana_toolbar_button_selected(_toolbar->parts_toggle); }
    else                          { kana_toolbar_button_plain(_toolbar->parts_toggle); }
}

// A row of chips from _left, each as wide as its label (and padding), centred
// on _y; squeezed together when they would pass _width.
RDE_INTERNAL void kana_toolbar_chip_row(kana_toolbar* _toolbar, rde_ui_button* const* _chips, const c8* const* _labels, u32 _count,
                                        f32 _left, f32 _width, f32 _y) {
    f32 _w[16];
    f32 _total = KANA_TOOLBAR_SPACING * (f32)(_count - 1u);
    for(u32 _i = 0; _i < _count && _i < 16u; _i++) {
        _w[_i]  = fmaxf(48.0f, kana_draw_text_width(_toolbar->font, (f32)KANA_TOOLBAR_FONT_SIZE, _labels[_i], KANA_TOOLBAR_TEXT_SCALE * (f32)KANA_TOOLBAR_FONT_SIZE) + 26.0f);
        _total += _w[_i];
    }
    const f32 _squeeze = _total > _width ? (_width - KANA_TOOLBAR_SPACING * (f32)(_count - 1u)) / (_total - KANA_TOOLBAR_SPACING * (f32)(_count - 1u)) : 1.0f;
    f32 _x = _left;
    for(u32 _i = 0; _i < _count && _i < 16u; _i++) {
        const f32 _cw = _w[_i] * _squeeze;
        kana_toolbar_place(rde_ui_button_as_node(_chips[_i]), (rde_vec_2F){ _x + _cw * 0.5f, _y }, (rde_vec_2F){ _cw, KANA_BROWSE_ROW_H });
        _x += _cw + KANA_TOOLBAR_SPACING;
    }
}

RDE_INTERNAL void kana_toolbar_layout_browse(kana_toolbar* _toolbar) {
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
    const f32        _pad    = KANA_TOOLBAR_PADDING + 4.0f;
    const f32        _gap    = KANA_TOOLBAR_SPACING;
    const f32        _left   = (f32)_insets.x + _pad;
    const f32        _width  = _screen.x - (f32)(_insets.x + _insets.z) - 2.0f * _pad;
    const f32        _height = (f32)_insets.y + _pad + KANA_BROWSE_ROW_H + _gap + KANA_BROWSE_ROW_H + _gap + KANA_BROWSE_FIELD_H + _pad;

    _toolbar->_browse_laid_out  = _screen;
    _toolbar->browse_bar_height = _height;

    // The bar spans the top, the status-bar strip included.
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->browse_bar), (rde_vec_2F){ _screen.x * 0.5f, _screen.y - _height * 0.5f }, (rde_vec_2F){ _screen.x, _height });

    // Rows, top to bottom (panel-local: bottom-left origin).
    f32 _y = _height - (f32)_insets.y - _pad - KANA_BROWSE_ROW_H * 0.5f;

    // Each chip as wide as its label; a row that would not fit is squeezed
    // (the labels shrink to fit their chips).
    kana_toolbar_chip_row(_toolbar, _toolbar->filter_chips, KANA_FILTER_LABELS, KANA_FILTER_COUNT, _left, _width, _y);
    _y -= KANA_BROWSE_ROW_H + _gap;
    kana_toolbar_chip_row(_toolbar, _toolbar->sort_chips, KANA_SORT_LABELS, KANA_SORT_COUNT, _left, _width, _y);

    _y -= (KANA_BROWSE_ROW_H + KANA_BROWSE_FIELD_H) * 0.5f + _gap;
    const f32 _field = _width - 3.0f * (KANA_BROWSE_SIDE_W + _gap);
    kana_toolbar_place(kana_toolbar_field_node(_toolbar->search_field), (rde_vec_2F){ _left + _field * 0.5f, _y }, (rde_vec_2F){ _field, KANA_BROWSE_FIELD_H });
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->draw_toggle), (rde_vec_2F){ _left + _field + _gap + KANA_BROWSE_SIDE_W * 0.5f, _y }, (rde_vec_2F){ KANA_BROWSE_SIDE_W, KANA_BROWSE_FIELD_H });
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->parts_toggle), (rde_vec_2F){ _left + _field + 2.0f * _gap + KANA_BROWSE_SIDE_W * 1.5f, _y }, (rde_vec_2F){ KANA_BROWSE_SIDE_W, KANA_BROWSE_FIELD_H });
    kana_toolbar_place(rde_ui_button_as_node(_toolbar->pad_clear), (rde_vec_2F){ _left + _width - KANA_BROWSE_SIDE_W * 0.5f, _y }, (rde_vec_2F){ KANA_BROWSE_SIDE_W, KANA_BROWSE_FIELD_H });
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_filter(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_chip_ref* _ref = (const kana_toolbar_chip_ref*)_user_data;
    kana_browse_set_filter(_ref->toolbar->browse, (KANA_FILTER_)_ref->index);
    kana_toolbar_refresh_browse(_ref->toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_sort(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_chip_ref* _ref = (const kana_toolbar_chip_ref*)_user_data;
    kana_browse_set_sort(_ref->toolbar->browse, (KANA_SORT_)_ref->index);
    kana_toolbar_refresh_browse(_ref->toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_browse_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_browse_close(_toolbar->browse);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_draw_toggle(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_browse_set_drawing(_toolbar->browse, !_toolbar->browse->drawing);
    kana_toolbar_refresh_browse(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Parts: the panel of parts instead of the pad (one or the other).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_parts_toggle(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_browse_set_picking(_toolbar->browse, !_toolbar->browse->picking);
    kana_toolbar_refresh_browse(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Clear: the drawing, the typed search and the picked parts.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_pad_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_toolbar->search_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_toolbar->search_field, 0, _bytes);
    }
    kana_browse_set_search(_toolbar->browse, "");
    kana_browse_clear_pad(_toolbar->browse);
    kana_browse_clear_parts(_toolbar->browse);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Every keystroke searches.
RDE_INTERNAL void kana_toolbar_on_search_changed(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    c8* _text = rde_ui_text_editor_get_text(_toolbar->search_field, 0, rde_ui_text_editor_get_byte_count(_toolbar->search_field));
    kana_browse_set_search(_toolbar->browse, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_toolbar->search_field, _text);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_prev(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_viewer_prev(((kana_toolbar*)_user_data)->viewer);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_replay(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_viewer_replay(((kana_toolbar*)_user_data)->viewer);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_next(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_viewer_next(((kana_toolbar*)_user_data)->viewer);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Back to Browse (or to the page, if the viewer was opened on its own).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_viewer_close(_toolbar->viewer);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}


RDE_INTERNAL void kana_toolbar_on_size(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->ink->constant_radius = rde_ui_slider_get_value(_toolbar->size);
}

// Grip drag. Position = where the bar was + how far the pointer has gone since the
// PRESS (not a sum of deltas: the movement before the drag threshold would be lost
// and the bar would trail the finger).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_drag_begin(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->drag_start_center = _toolbar->center;
    _toolbar->drag_press        = _info->press_position;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_drag_move(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->center = (rde_vec_2F){
        _toolbar->drag_start_center.x + (_info->position.x - _toolbar->drag_press.x),
        _toolbar->drag_start_center.y + (_info->position.y - _toolbar->drag_press.y)
    };
    _toolbar->center = kana_toolbar_clamp(_toolbar, _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place_popups(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap on the grip (a drag is not one: the engine drops the click once the grip
// drags). Two close together, in time and place, fold the bar or open it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_grip_tap(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    const f64     _now     = rde_engine_get_time_now();
    const f32     _dx      = _info->position.x - _toolbar->grip_tapped_at.x;
    const f32     _dy      = _info->position.y - _toolbar->grip_tapped_at.y;

    if(_toolbar->grip_tapped > 0.0 && _now - _toolbar->grip_tapped <= KANA_TOOLBAR_DOUBLE_TAP &&
       _dx * _dx + _dy * _dy <= KANA_TOOLBAR_DOUBLE_TAP_SLOP * KANA_TOOLBAR_DOUBLE_TAP_SLOP) {
        _toolbar->grip_tapped = 0.0;   // a third tap starts over
        kana_toolbar_set_minimized(_toolbar, !_toolbar->minimized);
    } else {
        _toolbar->grip_tapped    = _now;
        _toolbar->grip_tapped_at = _info->position;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the theme ---------------------------------------------------------------------

// Every widget's colours from the current theme: once at start, and again on a
// change of theme — nothing keeps a colour of its own.
RDE_INTERNAL void kana_toolbar_apply_theme(kana_toolbar* _toolbar) {
    const kana_theme* _t = kana_theme_active();

    kana_toolbar_style_panel(_toolbar->panel, KANA_TOOLBAR_RADIUS, 1.0f);
    kana_toolbar_style_panel(_toolbar->palette, 14.0f, 1.0f);
    kana_toolbar_style_panel(_toolbar->paper_panel, 14.0f, 1.0f);
    kana_toolbar_style_panel(_toolbar->word_card, 18.0f, 1.0f);
    kana_toolbar_button_colors(_toolbar->word_backdrop, KANA_TOOLBAR_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    for(u32 _i = 0; _i < 3u; _i++) {
        kana_toolbar_style_field(_toolbar->word_fields[_i]);
    }
    rde_ui_label_set_color(_toolbar->word_title, _t->button_text);
    rde_ui_label_set_color(_toolbar->word_error, _t->score_poor);
    kana_toolbar_style_panel(_toolbar->browse_bar, 0.0f, 1.0f);
    rde_ui_image_set_style(_toolbar->grip_area, RDE_UI_STATE_NORMAL, kana_toolbar_style((rde_color){ 0, 0, 0, 0 }, 0.0f));
    rde_ui_label_set_color(_toolbar->grip, _t->grip);
    for(u32 _i = 0; _i < KANA_TOOLBAR_SEPARATORS; _i++) {
        rde_ui_image_set_style(_toolbar->separators[_i], RDE_UI_STATE_NORMAL, kana_toolbar_style(_t->outline, 0.0f));
    }

    rde_ui_slider_set_track_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_toolbar_style(_t->slider_track, 3.0f));
    rde_ui_slider_set_fill_styles(_toolbar->size,  RDE_UI_STATE_NORMAL, kana_toolbar_style(_t->slider_fill, 3.0f));
    rde_ui_slider_set_thumb_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_toolbar_style(_t->slider_thumb, 7.0f));

    kana_toolbar_style_field(_toolbar->search_field);
    kana_toolbar_style_field(_toolbar->check_field);

    // The bar's tools are quiet; the ones that show a state are set after.
    rde_ui_button* const _tools[] = {
        _toolbar->undo, _toolbar->redo, _toolbar->draw, _toolbar->erase, _toolbar->lasso_tool, _toolbar->clear,
        _toolbar->brush_scale, _toolbar->paper, _toolbar->rotate, _toolbar->reset_view,
        _toolbar->paper_choices[0], _toolbar->paper_choices[1], _toolbar->paper_choices[2], _toolbar->paper_choices[3],
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        kana_toolbar_restyle_quiet(_tools[_i]);
    }
    rde_ui_button* const _plain[] = { _toolbar->word_cancel, _toolbar->draw_toggle, _toolbar->parts_toggle, _toolbar->pad_clear };
    for(u32 _i = 0; _i < sizeof(_plain) / sizeof(_plain[0]); _i++) {
        kana_toolbar_restyle_button(_plain[_i]);
    }

    // The rows: quiet buttons on a surface, the way on in the accent, Delete in red.
    kana_toolbar_menu* const _menus[] = { &_toolbar->selection_menu, &_toolbar->context_menu, &_toolbar->viewer_menu,
                                          &_toolbar->chart_menu, &_toolbar->browse_menu, &_toolbar->practice_menu, &_toolbar->check_menu,
                                          &_toolbar->album_menu, &_toolbar->album_page_menu, &_toolbar->practice_set_menu,
                                          &_toolbar->practice_summary_menu, &_toolbar->select_menu, &_toolbar->exam_setup_menu,
                                          &_toolbar->exam_preview_menu, &_toolbar->exam_menu, &_toolbar->exam_results_menu,
                                          &_toolbar->stats_menu, &_toolbar->viewer_add_menu };
    for(u32 _m = 0; _m < sizeof(_menus) / sizeof(_menus[0]); _m++) {
        kana_toolbar_style_panel(_menus[_m]->panel, KANA_TOOLBAR_ROW_RADIUS, 1.0f);
        for(u32 _i = 0; _i < _menus[_m]->count; _i++) {
            kana_toolbar_restyle_quiet(_menus[_m]->buttons[_i]);
            kana_toolbar_button_round(_menus[_m]->buttons[_i], 12.0f);
        }
    }
    kana_toolbar_button_danger_quiet(_toolbar->selection_menu.buttons[KANA_SELECTION_DELETE]);
    rde_ui_button* const _primary[] = {
        _toolbar->viewer_menu.buttons[KANA_VIEWER_PRACTICE],                 // the way on
        _toolbar->practice_menu.buttons[KANA_PRACTICE_SCORE],
        _toolbar->check_menu.buttons[KANA_CHECK_MENU_PRACTICE],
        _toolbar->album_page_menu.buttons[KANA_ALBUM_PAGE_PRACTICE],
        _toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_NEXT],
        _toolbar->practice_summary_menu.buttons[KANA_PRACTICE_SUMMARY_AGAIN],
        _toolbar->select_menu.buttons[KANA_SELECT_MENU_PRACTICE],
        _toolbar->exam_setup_menu.buttons[KANA_EXAM_SETUP_NEXT],
        _toolbar->exam_preview_menu.buttons[KANA_EXAM_PREVIEW_START],
        _toolbar->exam_menu.buttons[KANA_EXAM_MENU_NEXT],
        _toolbar->viewer_add_menu.buttons[KANA_VIEWER_ADD_DONE],
        _toolbar->browse_menu.buttons[KANA_BROWSE_MENU_PRACTICE],
        _toolbar->chart_menu.buttons[KANA_CHART_MENU_PRACTICE],
        _toolbar->album_menu.buttons[KANA_ALBUM_MENU_PRACTICE],
        _toolbar->exam_results_menu.buttons[KANA_EXAM_RESULTS_PRACTICE],
    };
    for(u32 _i = 0; _i < sizeof(_primary) / sizeof(_primary[0]); _i++) {
        kana_toolbar_button_primary(_primary[_i]);
        kana_toolbar_button_round(_primary[_i], 12.0f);
        kana_toolbar_set_enabled(_primary[_i], rde_ui_button_as_node(_primary[_i])->interactable);
    }
    kana_toolbar_button_selected(_toolbar->album_menu.buttons[_toolbar->album->sort]);         // the order shown
    kana_toolbar_button_round(_toolbar->album_menu.buttons[_toolbar->album->sort], 12.0f);
    kana_toolbar_button_primary(_toolbar->word_add);
    _toolbar->_mark_shown_for = 0;   // the viewer's Study, restyled for its mark again
    _toolbar->_exam_shown     = UINT32_MAX;
    kana_toolbar_show_guided(_toolbar);

    // The strip: no track, a thin thumb in the grip's colour.
    rde_ui_scroll_area_set_background_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_toolbar->strip, _t->grip, kana_toolbar_shade(_t->grip, 20), kana_toolbar_shade(_t->grip, 40));

    kana_side_apply_theme(_toolbar);

    // The swatches: round, ringed.
    for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
        kana_toolbar_button_colors(_toolbar->swatches[_i], kana_theme_resolve(KANA_TOOLBAR_PALETTE[_i]), 2.0f, _t->outline);
        kana_toolbar_button_round(_toolbar->swatches[_i], KANA_TOOLBAR_SWATCH * 0.5f);
    }

    kana_toolbar_refresh(_toolbar);          // the tools, the colour button
    kana_toolbar_refresh_browse(_toolbar);   // the chips
}

void kana_toolbar_sync(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL) {
        kana_toolbar_apply_theme(_toolbar);
    }
}

// --- lifetime ------------------------------------------------------------------------

void kana_toolbar_init(kana_toolbar* _toolbar, rde_window* _window, kana_ink* _ink, kana_canvas* _view, kana_lasso* _lasso,
                       kana_viewer* _viewer, kana_browse* _browse, kana_chart* _chart, kana_practice* _practice, kana_album* _album, kana_notes* _notes, kana_check* _check, b8* _show_hud) {
    memset(_toolbar, 0, sizeof(*_toolbar));
    _toolbar->window   = _window;
    _toolbar->ink      = _ink;
    _toolbar->view     = _view;
    _toolbar->lasso    = _lasso;
    _toolbar->viewer   = _viewer;
    _toolbar->browse   = _browse;
    _toolbar->chart    = _chart;
    _toolbar->practice = _practice;
    _toolbar->album    = _album;
    _toolbar->notes    = _notes;
    _toolbar->check    = _check;
    _toolbar->_album_practice_shown = UINT32_MAX;   // not shown yet: the first update sets it
    _toolbar->show_hud = _show_hud;
    _toolbar->tool     = KANA_TOOL_DRAW;
    _toolbar->vertical = true;

    rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _toolbar->font = rde_font_load(KANA_TOOLBAR_FONT_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);
    // Japanese falls through to Noto Sans JP (its glyphs load as they are first used).
    // Kanji need bigger budgets than Latin: with the defaults (128 curves) over a
    // quarter of Noto's kanji were silently left out (録, 鬱...). Measured over
    // every kana and kanji in the font (Slug's own curve split and bands): at
    // most 578 curve texels and 1,220 band texels. A glyph keeps its slot for
    // the life of the app, so there are enough slots for a long session of
    // words and readings. ~31 MB of GPU memory in all.
    rde_font_parameters _slug_jp = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _slug_jp.max_glyphs              = KANA_TOOLBAR_FONT_JP_GLYPHS;
    _slug_jp.curves_per_glyph_budget = 304u;    // 608 texels
    _slug_jp.bands_per_glyph_budget  = 1280u;
    _toolbar->font_jp = rde_font_load(KANA_TOOLBAR_FONT_JP_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug_jp, NULL);
    if(_toolbar->font != NULL && _toolbar->font_jp != NULL) {
        rde_font_add_fallback(_toolbar->font, _toolbar->font_jp);
    }
    // The icons (icons.h): Phosphor, last in line. Its busiest icon needs 540
    // curve texels and 875 band texels (measured over all 1,513); the app uses a
    // few dozen, and a font of its own for Fill.
    rde_font_parameters _slug_icons = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _slug_icons.max_glyphs              = 256u;
    _slug_icons.curves_per_glyph_budget = 288u;   // 576 texels
    _slug_icons.bands_per_glyph_budget  = 1024u;
    _toolbar->font_icons      = rde_font_load(KANA_TOOLBAR_FONT_ICONS_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug_icons, NULL);
    _toolbar->font_icons_fill = rde_font_load(KANA_TOOLBAR_FONT_ICONS_FILL_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug_icons, NULL);
    if(_toolbar->font != NULL && _toolbar->font_icons != NULL) {
        rde_font_add_fallback(_toolbar->font, _toolbar->font_icons);
    }
    kana_toolbar_icons_regular = _toolbar->font_icons != NULL ? _toolbar->font_icons : _toolbar->font;
    kana_toolbar_icons_fill    = _toolbar->font_icons_fill;
    kana_toolbar_glyph_font    = _toolbar->font;

    // CONSTANT_PIXEL: one UI unit = one window unit, so the bar is the same size
    // on every screen, and positions match the pen's.
    _toolbar->ui = rde_ui_canvas_create(_window, RDE_UI_CANVAS_RENDER_MODE_SCREEN_OVERLAY, NULL);
    rde_ui_canvas_set_scale_mode(_toolbar->ui, RDE_UI_CANVAS_SCALE_MODE_CONSTANT_PIXEL);
    // No automatic safe area. With it the canvas insets its ROOT — shifting every
    // child up by the bottom inset (the iPad's home-indicator strip) — so the bar
    // drew ~20 units above where kana_toolbar_hit tested it, and a pen pressing
    // the bar's top band wrote under it. The root must be exactly the window, the
    // space pen positions are in; kana_toolbar_clamp keeps the bar out of the
    // unsafe edges instead.
    rde_ui_canvas_set_safe_area_enabled(_toolbar->ui, false);
    rde_ui_node* _root = rde_ui_canvas_get_root(_toolbar->ui);

    // The panel is what moves; everything else is its child. blocks_input so a
    // press in a gap between buttons still counts as the toolbar's.
    // Colours are left to kana_toolbar_apply_theme, at the end.
    _toolbar->panel = rde_ui_image_create(NULL);
    {
        rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->panel), true);
        rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->panel));
    }
    rde_ui_node* _panel = rde_ui_image_as_node(_toolbar->panel);

    // The grip's end of the bar takes the drags and the taps, not the handle drawn
    // in it: a finger does not have to find the handle. A sibling of the strip, so
    // no tap on a tool ever counts towards a double tap.
    _toolbar->grip_area = rde_ui_image_create(NULL);
    {
        rde_ui_node* _a = rde_ui_image_as_node(_toolbar->grip_area);
        rde_ui_node_set_blocks_input(_a, true);
        rde_ui_node_set_user_data(_a, _toolbar);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, kana_toolbar_on_drag_begin);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  kana_toolbar_on_drag_move);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_CLICK,      kana_toolbar_on_grip_tap);
        rde_ui_node_add_child(_panel, _a);
    }
    _toolbar->grip = rde_ui_label_create(NULL);
    {
        rde_ui_label_set_font(_toolbar->grip, _toolbar->font_icons != NULL ? _toolbar->font_icons : _toolbar->font);
        rde_ui_label_set_font_scale(_toolbar->grip, KANA_TOOLBAR_GRIP_PX / (f32)KANA_TOOLBAR_FONT_SIZE);
        rde_ui_label_set_alignment(_toolbar->grip, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node* _g = rde_ui_label_as_node(_toolbar->grip);
        rde_ui_node_set_raycast_target(_g, false);
        rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->grip_area), _g);
    }

    // The tools, in a strip that scrolls along the bar (a drag that starts on a
    // button scrolls it; a tap presses the button).
    _toolbar->strip = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_toolbar->strip, 3.0f);
    rde_ui_node_add_child(_panel, rde_ui_scroll_area_as_node(_toolbar->strip));
    rde_ui_node* _tools = rde_ui_scroll_area_as_node(_toolbar->strip);

    // The tools are icons (their names kept as labels, hidden).
    _toolbar->undo  = kana_toolbar_button(_toolbar, _tools, "Undo",  kana_toolbar_on_undo);
    _toolbar->redo  = kana_toolbar_button(_toolbar, _tools, "Redo",  kana_toolbar_on_redo);
    _toolbar->draw  = kana_toolbar_button(_toolbar, _tools, "Draw",  kana_toolbar_on_draw);
    _toolbar->erase = kana_toolbar_button(_toolbar, _tools, "Erase", kana_toolbar_on_erase);
    _toolbar->lasso_tool = kana_toolbar_button(_toolbar, _tools, "Lasso", kana_toolbar_on_lasso);
    _toolbar->clear = kana_toolbar_button(_toolbar, _tools, "Clear", kana_toolbar_on_clear);
    for(u32 _i = 0; _i < KANA_TOOLBAR_SEPARATORS; _i++) {
        _toolbar->separators[_i] = rde_ui_image_create(NULL);
        rde_ui_node_set_raycast_target(rde_ui_image_as_node(_toolbar->separators[_i]), false);
        rde_ui_node_add_child(_tools, rde_ui_image_as_node(_toolbar->separators[_i]));
    }

    _toolbar->size = rde_ui_slider_create(NULL);
    {
        rde_ui_node* _n = rde_ui_slider_as_node(_toolbar->size);
        rde_ui_slider_set_range(_toolbar->size, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
        rde_ui_slider_set_step(_toolbar->size, 0.5f);
        rde_ui_node_set_user_data(_n, _toolbar);
        rde_ui_slider_set_on_value_changed(_toolbar->size, kana_toolbar_on_size);
        rde_ui_node_add_child(_tools, _n);
    }

    _toolbar->color       = kana_toolbar_button(_toolbar, _tools, "Color",  kana_toolbar_on_color);
    _toolbar->brush_scale = kana_toolbar_button(_toolbar, _tools, "Page",   kana_toolbar_on_brush_scale);
    _toolbar->paper       = kana_toolbar_button(_toolbar, _tools, "Paper",  kana_toolbar_on_paper);
    _toolbar->rotate      = kana_toolbar_button(_toolbar, _tools, "Rotate", kana_toolbar_on_rotate);
    _toolbar->reset_view  = kana_toolbar_button(_toolbar, _tools, "Reset",  kana_toolbar_on_reset_view);
    {
        const struct { rde_ui_button* button; const c8* icon; } _icons[] = {
            { _toolbar->undo, KANA_ICON_UNDO }, { _toolbar->redo, KANA_ICON_REDO }, { _toolbar->draw, KANA_ICON_DRAW },
            { _toolbar->erase, KANA_ICON_ERASE }, { _toolbar->lasso_tool, KANA_ICON_LASSO }, { _toolbar->clear, KANA_ICON_TRASH },
            { _toolbar->brush_scale, KANA_ICON_PAGE }, { _toolbar->paper, KANA_ICON_PAPER_DOTS }, { _toolbar->rotate, KANA_ICON_ROTATE },
            { _toolbar->reset_view, KANA_ICON_RESET_VIEW },
        };
        for(u32 _i = 0; _i < sizeof(_icons) / sizeof(_icons[0]); _i++) {
            kana_toolbar_icon(_icons[_i].button, _icons[_i].icon, KANA_TOOLBAR_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);
        }
        // Color shows the colour itself: a dot in the button.
        rde_ui_button_set_text(_toolbar->color, NULL);
        _toolbar->color_dot = rde_ui_image_create(NULL);
        rde_ui_node* _dot = rde_ui_image_as_node(_toolbar->color_dot);
        rde_ui_node_set_raycast_target(_dot, false);
        rde_ui_node_add_child(rde_ui_button_as_node(_toolbar->color), _dot);
        kana_toolbar_place(_dot, (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W * 0.5f, KANA_TOOLBAR_BUTTON_H * 0.5f }, (rde_vec_2F){ KANA_TOOLBAR_DOT, KANA_TOOLBAR_DOT });
    }

    // The palette is its own panel under the root, so it can sit outside the bar.
    _toolbar->palette = rde_ui_image_create(NULL);
    {
        rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->palette), true);
        rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->palette));

        for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
            _toolbar->swatch_refs[_i] = (kana_toolbar_swatch_ref){ _toolbar, _i };
            _toolbar->swatches[_i]    = rde_ui_button_create(NULL, NULL);
            rde_ui_button_set_on_click(_toolbar->swatches[_i], kana_toolbar_on_swatch, &_toolbar->swatch_refs[_i]);
            rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->palette), rde_ui_button_as_node(_toolbar->swatches[_i]));
        }
    }

    // The paper panel, the same way.
    _toolbar->paper_panel = rde_ui_image_create(NULL);
    {
        rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->paper_panel), true);
        rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->paper_panel));
        static const c8* const _names[KANA_PAPER_COUNT] = { "Dots", "Squares", "Lines", "None" };   // KANA_PAPER_ order
        static const c8* const _icons[KANA_PAPER_COUNT] = { KANA_ICON_PAPER_DOTS, KANA_ICON_PAPER_SQUARES, KANA_ICON_PAPER_LINES, KANA_ICON_PAPER_NONE };
        for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
            _toolbar->paper_refs[_i]    = (kana_toolbar_swatch_ref){ _toolbar, _i };
            _toolbar->paper_choices[_i] = kana_toolbar_button(_toolbar, rde_ui_image_as_node(_toolbar->paper_panel), _names[_i], kana_toolbar_on_paper_choice);
            rde_ui_button_set_on_click(_toolbar->paper_choices[_i], kana_toolbar_on_paper_choice, &_toolbar->paper_refs[_i]);
            kana_toolbar_icon(_toolbar->paper_choices[_i], _icons[_i], KANA_TOOLBAR_ICON_ABOVE, 18.0f);
        }
    }

    // The menus: panels of their own under the root. The selection menu is placed
    // over the selection by kana_toolbar_update; the context menu opens at a
    // long press.
    {
        const c8* const             _labels[KANA_SELECTION_COUNT]    = { "Cut", "Copy", "Duplicate", "Check", "Delete" };
        const c8* const             _icons[KANA_SELECTION_COUNT]     = { KANA_ICON_CUT, KANA_ICON_COPY, KANA_ICON_DUPLICATE, KANA_ICON_SEARCH, KANA_ICON_TRASH };
        const rde_ui_event_callback _callbacks[KANA_SELECTION_COUNT] = { kana_toolbar_on_cut, kana_toolbar_on_copy, kana_toolbar_on_duplicate, kana_toolbar_on_check,
                                                                         kana_toolbar_on_delete_selection };
        kana_toolbar_menu_create(_toolbar, &_toolbar->selection_menu, _root, _labels, _icons, _callbacks, KANA_SELECTION_COUNT);
    }
    {
        const c8* const             _labels[KANA_CONTEXT_COUNT]    = { "Paste", "Select all" };
        const c8* const             _icons[KANA_CONTEXT_COUNT]     = { KANA_ICON_PASTE, KANA_ICON_SELECT_ALL };
        const rde_ui_event_callback _callbacks[KANA_CONTEXT_COUNT] = { kana_toolbar_on_paste, kana_toolbar_on_select_all };
        kana_toolbar_menu_create(_toolbar, &_toolbar->context_menu, _root, _labels, _icons, _callbacks, KANA_CONTEXT_COUNT);
    }
    {
        const c8* const             _labels[KANA_VIEWER_COUNT]    = { "Back", "Prev", "Replay", "Next", "Study", "Practice" };
        const c8* const             _icons[KANA_VIEWER_COUNT]     = { KANA_ICON_BACK, KANA_ICON_PREV, KANA_ICON_REPLAY, KANA_ICON_NEXT, KANA_ICON_STAR, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_VIEWER_COUNT] = { kana_toolbar_on_viewer_back, kana_toolbar_on_viewer_prev, kana_toolbar_on_viewer_replay,
                                                                      kana_toolbar_on_viewer_next, kana_toolbar_on_viewer_study, kana_toolbar_on_viewer_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->viewer_menu, _root, _labels, _icons, _callbacks, KANA_VIEWER_COUNT);
    }
    {
        const c8* const             _labels[KANA_CHECK_MENU_COUNT]    = { "Back", "Stroke order", "Practice" };
        const c8* const             _icons[KANA_CHECK_MENU_COUNT]     = { KANA_ICON_BACK, KANA_ICON_STROKE_ORDER, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_CHECK_MENU_COUNT] = { kana_toolbar_on_check_back, kana_toolbar_on_check_order, kana_toolbar_on_check_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->check_menu, _root, _labels, _icons, _callbacks, KANA_CHECK_MENU_COUNT);

        _toolbar->check_field = rde_ui_text_editor_create(_toolbar->font, NULL);
        rde_ui_text_editor_set_multiline(_toolbar->check_field, false);
        rde_ui_text_editor_set_font_size(_toolbar->check_field, KANA_TOOLBAR_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_toolbar->check_field, KANA_CHECK_MEANT);
        rde_ui_text_editor_set_placeholder(_toolbar->check_field, "I meant… (kyou wa, or Japanese)");
        rde_ui_text_editor_set_content_insets(_toolbar->check_field, 12.0f, 8.0f, 12.0f, 8.0f);
        rde_ui_text_editor_add_plugin(_toolbar->check_field, rde_ui_text_editor_plugin_ime_get());   // the keyboard's composition, at the caret
        rde_ui_node* _field = rde_ui_text_editor_as_node(_toolbar->check_field);
        rde_ui_node_set_user_data(_field, _toolbar);
        rde_ui_text_editor_set_on_submit(_toolbar->check_field, kana_toolbar_on_check_meant);
        kana_toolbar_field_box(_root, _toolbar->check_field);
        rde_ui_node_set_active(kana_toolbar_field_node(_toolbar->check_field), false);
    }
    {
        const c8* const             _labels[KANA_CHART_MENU_COUNT]    = { "Hiragana", "Katakana", "Select", "Practice", "Close" };
        const c8* const             _icons[KANA_CHART_MENU_COUNT]     = { "\xE3\x81\x82", "\xE3\x82\xA2", KANA_ICON_SELECT, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_CHART_MENU_COUNT] = { kana_toolbar_on_chart_hiragana, kana_toolbar_on_chart_katakana, kana_toolbar_on_select_mode,
                                                                          kana_toolbar_on_chart_practice, kana_toolbar_on_chart_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->chart_menu, _root, _labels, _icons, _callbacks, KANA_CHART_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_BROWSE_MENU_COUNT]    = { "Select", "Practice", "Close" };
        const c8* const             _icons[KANA_BROWSE_MENU_COUNT]     = { KANA_ICON_SELECT, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_BROWSE_MENU_COUNT] = { kana_toolbar_on_select_mode, kana_toolbar_on_browse_practice, kana_toolbar_on_browse_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->browse_menu, _root, _labels, _icons, _callbacks, KANA_BROWSE_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_SELECT_MENU_COUNT]    = { "All", "None", "Study", "Exam", "Practice 0", "Done" };
        const c8* const             _icons[KANA_SELECT_MENU_COUNT]     = { KANA_ICON_SELECT_ALL, KANA_ICON_SELECT_NONE, KANA_ICON_STAR, KANA_ICON_EXAM, KANA_ICON_PEN, KANA_ICON_CHECK };
        const rde_ui_event_callback _callbacks[KANA_SELECT_MENU_COUNT] = { kana_toolbar_on_ticks_all, kana_toolbar_on_ticks_none, kana_toolbar_on_ticks_study,
                                                                           kana_toolbar_on_ticks_exam, kana_toolbar_on_ticks_practice, kana_toolbar_on_ticks_done };
        kana_toolbar_menu_create(_toolbar, &_toolbar->select_menu, _root, _labels, _icons, _callbacks, KANA_SELECT_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_SETUP_COUNT]    = { "Close", "Next 0" };
        const c8* const             _icons[KANA_EXAM_SETUP_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_EXAM_SETUP_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_next_stage };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_setup_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_SETUP_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_PREVIEW_COUNT]    = { "Back", "All", "None", "Start 0" };
        const c8* const             _icons[KANA_EXAM_PREVIEW_COUNT]     = { KANA_ICON_BACK, KANA_ICON_SELECT_ALL, KANA_ICON_SELECT_NONE, KANA_ICON_PLAY };
        const rde_ui_event_callback _callbacks[KANA_EXAM_PREVIEW_COUNT] = { kana_toolbar_on_exam_back, kana_toolbar_on_exam_all, kana_toolbar_on_exam_none,
                                                                            kana_toolbar_on_exam_start };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_preview_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_PREVIEW_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_MENU_COUNT]    = { "Quit", "Undo", "Clear", "Next" };
        const c8* const             _icons[KANA_EXAM_MENU_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_EXAM_MENU_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_undo, kana_toolbar_on_exam_clear,
                                                                         kana_toolbar_on_exam_next };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_RESULTS_COUNT]    = { "Done", "Retry wrong", "Practice wrong" };
        const c8* const             _icons[KANA_EXAM_RESULTS_COUNT]     = { KANA_ICON_CHECK, KANA_ICON_RETRY, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_EXAM_RESULTS_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_retry, kana_toolbar_on_exam_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_results_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_RESULTS_COUNT);
    }
    {
        const c8* const             _labels[KANA_STATS_MENU_COUNT]    = { "Close" };
        const c8* const             _icons[KANA_STATS_MENU_COUNT]     = { KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_STATS_MENU_COUNT] = { kana_toolbar_on_stats_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->stats_menu, _root, _labels, _icons, _callbacks, KANA_STATS_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_VIEWER_ADD_COUNT]    = { "Done", "Type your own" };
        const c8* const             _icons[KANA_VIEWER_ADD_COUNT]     = { KANA_ICON_CHECK, KANA_ICON_KEYBOARD };
        const rde_ui_event_callback _callbacks[KANA_VIEWER_ADD_COUNT] = { kana_toolbar_on_viewer_add_done, kana_toolbar_on_viewer_add_type };
        kana_toolbar_menu_create(_toolbar, &_toolbar->viewer_add_menu, _root, _labels, _icons, _callbacks, KANA_VIEWER_ADD_COUNT);
    }
    {
        // The form for a word typed in: over everything, with a backdrop that
        // cancels it.
        _toolbar->word_backdrop = rde_ui_button_create(NULL, NULL);
        rde_ui_button_set_on_click(_toolbar->word_backdrop, kana_toolbar_on_word_cancel, _toolbar);
        rde_ui_node_add_child(_root, rde_ui_button_as_node(_toolbar->word_backdrop));
        _toolbar->word_card = rde_ui_image_create(NULL);
        rde_ui_node* _card  = rde_ui_image_as_node(_toolbar->word_card);
        rde_ui_node_set_blocks_input(_card, true);
        rde_ui_node_add_child(_root, _card);
        _toolbar->word_title = rde_ui_label_create(NULL);
        rde_ui_label_set_font(_toolbar->word_title, _toolbar->font);
        rde_ui_label_set_font_scale(_toolbar->word_title, 17.0f / (f32)KANA_TOOLBAR_FONT_SIZE);
        rde_ui_label_set_alignment(_toolbar->word_title, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node_set_raycast_target(rde_ui_label_as_node(_toolbar->word_title), false);
        rde_ui_node_add_child(_card, rde_ui_label_as_node(_toolbar->word_title));
        const c8* const _placeholders[3] = { "Word", "Reading: kana, or romaji (megusuri)", "Meaning (optional)" };
        for(u32 _i = 0; _i < 3u; _i++) {
            rde_ui_text_editor* _f = rde_ui_text_editor_create(_toolbar->font, NULL);
            rde_ui_text_editor_set_multiline(_f, false);
            rde_ui_text_editor_set_font_size(_f, KANA_TOOLBAR_FIELD_PX);
            rde_ui_text_editor_set_max_chars(_f, _i == 2u ? 60u : 24u);
            rde_ui_text_editor_set_placeholder(_f, _placeholders[_i]);
            rde_ui_text_editor_set_content_insets(_f, 12.0f, 8.0f, 12.0f, 8.0f);
            rde_ui_text_editor_add_plugin(_f, rde_ui_text_editor_plugin_ime_get());   // Japanese from the keyboard
            rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), _toolbar);
            rde_ui_text_editor_set_on_submit(_f, kana_toolbar_on_word_field);
            kana_toolbar_field_box(_card, _f);
            _toolbar->word_fields[_i] = _f;
        }
        _toolbar->word_error = rde_ui_label_create(NULL);
        rde_ui_label_set_font(_toolbar->word_error, _toolbar->font);
        rde_ui_label_set_font_scale(_toolbar->word_error, 12.0f / (f32)KANA_TOOLBAR_FONT_SIZE);
        rde_ui_label_set_alignment(_toolbar->word_error, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node_set_raycast_target(rde_ui_label_as_node(_toolbar->word_error), false);
        rde_ui_node_add_child(_card, rde_ui_label_as_node(_toolbar->word_error));
        _toolbar->word_cancel = kana_toolbar_button(_toolbar, _card, "Cancel", kana_toolbar_on_word_cancel);
        _toolbar->word_add    = kana_toolbar_button(_toolbar, _card, KANA_ICON_PLUS "  Add", kana_toolbar_on_word_add);
        kana_toolbar_close_word_form(_toolbar);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_COUNT]    = { "Back", "Undo", "Clear", "Score", "Fewer", "More", "Guided" };
        const c8* const             _icons[KANA_PRACTICE_COUNT]     = { KANA_ICON_BACK, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_TARGET, KANA_ICON_MINUS, KANA_ICON_PLUS, KANA_ICON_GUIDED };
        const rde_ui_event_callback _callbacks[KANA_PRACTICE_COUNT] = { kana_toolbar_on_practice_back, kana_toolbar_on_practice_undo, kana_toolbar_on_practice_clear,
                                                                        kana_toolbar_on_practice_score, kana_toolbar_on_practice_fewer, kana_toolbar_on_practice_more,
                                                                        kana_toolbar_on_practice_guided };
        kana_toolbar_menu_create(_toolbar, &_toolbar->practice_menu, _root, _labels, _icons, _callbacks, KANA_PRACTICE_COUNT);
    }
    {
        const c8* const             _labels[KANA_ALBUM_MENU_COUNT]    = { "Weakest", "Recent", "Most", "Practice", "Close" };
        const c8* const             _icons[KANA_ALBUM_MENU_COUNT]     = { KANA_ICON_WEAKEST, KANA_ICON_CLOCK, KANA_ICON_STACK, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_ALBUM_MENU_COUNT] = { kana_toolbar_on_album_sort, kana_toolbar_on_album_sort, kana_toolbar_on_album_sort,
                                                                          kana_toolbar_on_album_practice_weakest, kana_toolbar_on_album_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->album_menu, _root, _labels, _icons, _callbacks, KANA_ALBUM_MENU_COUNT);
        for(u32 _i = 0; _i < KANA_ALBUM_SORT_COUNT; _i++) {
            _toolbar->album_sort_refs[_i] = (kana_toolbar_chip_ref){ _toolbar, _i };
            rde_ui_button_set_on_click(_toolbar->album_menu.buttons[_i], kana_toolbar_on_album_sort, &_toolbar->album_sort_refs[_i]);
        }
    }
    {
        const c8* const             _labels[KANA_ALBUM_PAGE_COUNT]    = { "Back", "Practice" };
        const c8* const             _icons[KANA_ALBUM_PAGE_COUNT]     = { KANA_ICON_BACK, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_ALBUM_PAGE_COUNT] = { kana_toolbar_on_album_back, kana_toolbar_on_album_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->album_page_menu, _root, _labels, _icons, _callbacks, KANA_ALBUM_PAGE_COUNT);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_SET_COUNT]    = { "Back", "Undo", "Clear", "Score", "Guided", "Next" };
        const c8* const             _icons[KANA_PRACTICE_SET_COUNT]     = { KANA_ICON_BACK, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_TARGET, KANA_ICON_GUIDED, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_PRACTICE_SET_COUNT] = { kana_toolbar_on_practice_done, kana_toolbar_on_practice_undo, kana_toolbar_on_practice_clear,
                                                                            kana_toolbar_on_practice_score, kana_toolbar_on_practice_guided, kana_toolbar_on_practice_next };
        kana_toolbar_menu_create(_toolbar, &_toolbar->practice_set_menu, _root, _labels, _icons, _callbacks, KANA_PRACTICE_SET_COUNT);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_SUMMARY_COUNT]    = { "Weakest again", "Done" };
        const c8* const             _icons[KANA_PRACTICE_SUMMARY_COUNT]     = { KANA_ICON_RETRY, KANA_ICON_CHECK };
        const rde_ui_event_callback _callbacks[KANA_PRACTICE_SUMMARY_COUNT] = { kana_toolbar_on_practice_again, kana_toolbar_on_practice_done };
        kana_toolbar_menu_create(_toolbar, &_toolbar->practice_summary_menu, _root, _labels, _icons, _callbacks, KANA_PRACTICE_SUMMARY_COUNT);
    }

    // Browse's bar: a panel across the top; laid out when first shown (and when
    // the screen rotates), since it spans the screen.
    _toolbar->browse_bar = rde_ui_image_create(NULL);
    {
        rde_ui_node* _bar = rde_ui_image_as_node(_toolbar->browse_bar);
        rde_ui_node_set_blocks_input(_bar, true);
        rde_ui_node_add_child(_root, _bar);

        for(u32 _i = 0; _i < KANA_FILTER_COUNT; _i++) {
            _toolbar->filter_refs[_i]  = (kana_toolbar_chip_ref){ _toolbar, _i };
            _toolbar->filter_chips[_i] = kana_toolbar_button(_toolbar, _bar, KANA_FILTER_LABELS[_i], kana_toolbar_on_filter);
            rde_ui_button_set_on_click(_toolbar->filter_chips[_i], kana_toolbar_on_filter, &_toolbar->filter_refs[_i]);
        }
        for(u32 _i = 0; _i < KANA_SORT_COUNT; _i++) {
            _toolbar->sort_refs[_i]  = (kana_toolbar_chip_ref){ _toolbar, _i };
            _toolbar->sort_chips[_i] = kana_toolbar_button(_toolbar, _bar, KANA_SORT_LABELS[_i], kana_toolbar_on_sort);
            rde_ui_button_set_on_click(_toolbar->sort_chips[_i], kana_toolbar_on_sort, &_toolbar->sort_refs[_i]);
        }
        _toolbar->draw_toggle  = kana_toolbar_button(_toolbar, _bar, "Draw",  kana_toolbar_on_draw_toggle);
        _toolbar->parts_toggle = kana_toolbar_button(_toolbar, _bar, "Parts", kana_toolbar_on_parts_toggle);
        if(!kana_browse_parts_available(_browse)) {
            kana_toolbar_set_enabled(_toolbar->parts_toggle, false);   // character data baked before parts
        }
        _toolbar->pad_clear    = kana_toolbar_button(_toolbar, _bar, "Clear", kana_toolbar_on_pad_clear);
        kana_toolbar_icon(_toolbar->draw_toggle, KANA_ICON_SCRIBBLE, KANA_TOOLBAR_ICON_LEFT, 15.0f);
        kana_toolbar_icon(_toolbar->parts_toggle, "\xE9\x83\xA8", KANA_TOOLBAR_ICON_LEFT, 13.0f);   // 部
        kana_toolbar_icon(_toolbar->pad_clear, KANA_ICON_CLOSE, KANA_TOOLBAR_ICON_LEFT, 14.0f);

        _toolbar->search_field = rde_ui_text_editor_create(_toolbar->font, NULL);
        rde_ui_text_editor_set_multiline(_toolbar->search_field, false);
        rde_ui_text_editor_set_font_size(_toolbar->search_field, KANA_TOOLBAR_FIELD_PX);
        rde_ui_text_editor_set_placeholder(_toolbar->search_field, KANA_ICON_SEARCH "  A meaning or a reading: tree, moku");
        rde_ui_text_editor_set_content_insets(_toolbar->search_field, 12.0f, 8.0f, 12.0f, 8.0f);
        rde_ui_node* _field = rde_ui_text_editor_as_node(_toolbar->search_field);
        rde_ui_node_set_user_data(_field, _toolbar);
        rde_ui_text_editor_set_on_change(_toolbar->search_field, kana_toolbar_on_search_changed);
        kana_toolbar_field_box(_bar, _toolbar->search_field);

        rde_ui_node_set_active(_bar, false);
    }

    // The side panel and Settings last: over everything else.
    kana_side_create(_toolbar, _root);

    // Start on the right edge, vertically centred.
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    _toolbar->center = (rde_vec_2F){ _screen.x - 60.0f, _screen.y * 0.5f };

    kana_toolbar_layout(_toolbar);
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_set_paper_open(_toolbar, false);
    kana_toolbar_apply_theme(_toolbar);
    kana_toolbar_update(_toolbar);
}

void kana_toolbar_destroy(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL) {
        rde_ui_canvas_destroy(_toolbar->ui);
        _toolbar->ui = NULL;
    }

    if(_toolbar->font != NULL) {
        rde_font_clear_fallbacks(_toolbar->font);
        rde_font_unload(_toolbar->font);
        _toolbar->font = NULL;
    }
    rde_font** const _fonts[] = { &_toolbar->font_jp, &_toolbar->font_icons, &_toolbar->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(*_fonts[_i] != NULL) {
            rde_font_unload(*_fonts[_i]);
            *_fonts[_i] = NULL;
        }
    }
}

RDE_INTERNAL b8 kana_toolbar_rect_contains(rde_vec_2F _center, rde_vec_2F _size, rde_vec_2F _p) {
    return _p.x >= _center.x - _size.x * 0.5f && _p.x <= _center.x + _size.x * 0.5f &&
           _p.y >= _center.y - _size.y * 0.5f && _p.y <= _center.y + _size.y * 0.5f;
}

b8 kana_toolbar_hit(const kana_toolbar* _toolbar, rde_vec_2F _screen) {
    if(_toolbar->ui == NULL) {
        return false;
    }

    // Kana screen space (centre origin) → UI canvas space (bottom-left origin).
    const rde_vec_2F _half = { kana_toolbar_virtual_size(_toolbar).x * 0.5f, kana_toolbar_virtual_size(_toolbar).y * 0.5f };
    const rde_vec_2F _p    = { _screen.x + _half.x, _screen.y + _half.y };

    // The side panel and Settings (while either is open, the whole screen), and the word form.
    if(kana_side_hit(_toolbar, _p) || _toolbar->word_open) {
        return true;
    }

    // The floating bar only while it shows: under a full-screen scene it is hidden
    // but keeps its rect, and a press starting there was swallowed as the bar's.
    if(!_toolbar->_viewer_shown && kana_toolbar_rect_contains(_toolbar->center, _toolbar->panel_size, _p)) {
        return true;
    }

    // Kept by kana_toolbar_place_palette, not read back from the UI: the computed
    // rect only updates at the next layout pass.
    if(_toolbar->palette_open && kana_toolbar_rect_contains(_toolbar->palette_center, _toolbar->palette_size, _p)) {
        return true;
    }
    if(_toolbar->paper_open && kana_toolbar_rect_contains(_toolbar->paper_center, _toolbar->paper_size, _p)) {
        return true;
    }

    // Check's field.
    if(_toolbar->_check_field_shown) {
        const rde_vec_2F _size = KANA_TOOLBAR_CHECK_FIELD;
        const rde_vec_2F _vs   = kana_toolbar_virtual_size(_toolbar);
        const rde_vec_4I _ins  = rde_window_get_safe_area_insets(_toolbar->window);
        if(_p.x >= _vs.x - (f32)_ins.z - 16.0f - _size.x && _p.y >= _vs.y - (f32)_ins.y - 8.0f - _size.y) {
            return true;
        }
    }

    // Browse's bar: everything above its bottom edge.
    if(_toolbar->_browse_shown && _p.y >= kana_toolbar_virtual_size(_toolbar).y - _toolbar->browse_bar_height) {
        return true;
    }

    const kana_toolbar_menu* _menus[] = { &_toolbar->selection_menu, &_toolbar->context_menu, &_toolbar->viewer_menu,
                                          &_toolbar->chart_menu, &_toolbar->browse_menu, &_toolbar->practice_menu, &_toolbar->check_menu,
                                          &_toolbar->album_menu, &_toolbar->album_page_menu, &_toolbar->practice_set_menu,
                                          &_toolbar->practice_summary_menu, &_toolbar->select_menu, &_toolbar->exam_setup_menu,
                                          &_toolbar->exam_preview_menu, &_toolbar->exam_menu, &_toolbar->exam_results_menu,
                                          &_toolbar->stats_menu, &_toolbar->viewer_add_menu };
    for(u32 _i = 0; _i < sizeof(_menus) / sizeof(_menus[0]); _i++) {
        if(_menus[_i]->open && kana_toolbar_rect_contains(_menus[_i]->center, _menus[_i]->size, _p)) {
            return true;
        }
    }

    return false;
}
