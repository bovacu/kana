#include "toolbar.h"
#include "vocab.h"
#include "review.h"
#include "theme.h"
#include "toolbar_kit.h"
#include "draw.h"
#include "icons.h"
#include "text.h"
#include "mlkit.h"
#include "speech.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See toolbar.h.
// ===========================================================================

#define KANA_TOOLBAR_FONT_PATH   "assets/fonts/Roboto-Regular.ttf"
#define KANA_TOOLBAR_FONT_JP_PATH "assets/fonts/NotoSansJP-Regular.otf"
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
// Translate with Google's card by the selection.
#define KANA_TOOLBAR_CARD_W      440.0f   // at most (narrower screens: what fits)
#define KANA_TOOLBAR_CARD_PAD    14.0f
#define KANA_TOOLBAR_CARD_FROM   16.0f    // what was read: size and line
#define KANA_TOOLBAR_CARD_FROM_L 22.0f
#define KANA_TOOLBAR_CARD_TO     18.0f    // its translation
#define KANA_TOOLBAR_CARD_TO_L   25.0f
#define KANA_TOOLBAR_CARD_WORD   26.0f    // a word's row under the translation
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
#define KANA_TOOLBAR_TEXT_CELL     72.0f  // Paste text: a character's cell, screen points at the zoom it is pasted at
#define KANA_TOOLBAR_TEXT_WIDTH    0.8f   // ...and its lines no wider than this much of the screen
#define KANA_TOOLBAR_NOTICE_CHARS  16u    // Copy as text says what it copied up to this long; more, how many

// Button order in each menu.
// Translate with Google only where there is a translator (translate.h).
enum { KANA_SELECTION_CUT = 0, KANA_SELECTION_COPY, KANA_SELECTION_COPY_TEXT, KANA_SELECTION_TRANSLATE, KANA_SELECTION_SAVE_WORD, KANA_SELECTION_DUPLICATE,
       KANA_SELECTION_CHECK, KANA_SELECTION_DELETE, KANA_SELECTION_COUNT };
enum { KANA_CHECK_MENU_BACK = 0, KANA_CHECK_MENU_ORDER, KANA_CHECK_MENU_PRACTICE, KANA_CHECK_MENU_COUNT };
enum { KANA_CONTEXT_PASTE = 0, KANA_CONTEXT_PASTE_TEXT, KANA_CONTEXT_SCAN, KANA_CONTEXT_SELECT_ALL, KANA_CONTEXT_COUNT };
enum { KANA_VIEWER_BACK = 0, KANA_VIEWER_PREV, KANA_VIEWER_REPLAY, KANA_VIEWER_NEXT, KANA_VIEWER_STUDY, KANA_VIEWER_SHEET, KANA_VIEWER_PRACTICE, KANA_VIEWER_COUNT };
enum { KANA_CHART_MENU_HIRAGANA = 0, KANA_CHART_MENU_KATAKANA, KANA_CHART_MENU_SELECT, KANA_CHART_MENU_PRACTICE, KANA_CHART_MENU_CLOSE, KANA_CHART_MENU_COUNT };
enum { KANA_BROWSE_MENU_SELECT = 0, KANA_BROWSE_MENU_PRACTICE, KANA_BROWSE_MENU_CLOSE, KANA_BROWSE_MENU_COUNT };
enum { KANA_SELECT_MENU_ALL = 0, KANA_SELECT_MENU_NONE, KANA_SELECT_MENU_STUDY, KANA_SELECT_MENU_EXAM, KANA_SELECT_MENU_SHEET, KANA_SELECT_MENU_LIST, KANA_SELECT_MENU_PRACTICE,
       KANA_SELECT_MENU_DONE, KANA_SELECT_MENU_COUNT };
enum { KANA_EXAM_SETUP_CLOSE = 0, KANA_EXAM_SETUP_NEXT, KANA_EXAM_SETUP_COUNT };
enum { KANA_EXAM_PREVIEW_BACK = 0, KANA_EXAM_PREVIEW_ALL, KANA_EXAM_PREVIEW_NONE, KANA_EXAM_PREVIEW_START, KANA_EXAM_PREVIEW_COUNT };
enum { KANA_EXAM_MENU_QUIT = 0, KANA_EXAM_MENU_UNDO, KANA_EXAM_MENU_CLEAR, KANA_EXAM_MENU_NEXT, KANA_EXAM_MENU_COUNT };
enum { KANA_EXAM_RESULTS_DONE = 0, KANA_EXAM_RESULTS_RETRY, KANA_EXAM_RESULTS_PRACTICE, KANA_EXAM_RESULTS_COUNT };
enum { KANA_STATS_MENU_CLOSE = 0, KANA_STATS_MENU_COUNT };
// Translate with Google only where there is a translator (translate.h): elsewhere the row has no such button.
enum { KANA_VOCAB_MENU_BACK = 0, KANA_VOCAB_MENU_WORD, KANA_VOCAB_MENU_REVIEW, KANA_VOCAB_MENU_EXAM, KANA_VOCAB_MENU_SHEET, KANA_VOCAB_MENU_PRACTICE,
       KANA_VOCAB_MENU_COUNT };
enum { KANA_WORDEXAM_SETUP_CLOSE = 0, KANA_WORDEXAM_SETUP_START, KANA_WORDEXAM_SETUP_COUNT };
enum { KANA_WORDEXAM_MENU_QUIT = 0, KANA_WORDEXAM_MENU_UNDO, KANA_WORDEXAM_MENU_CLEAR, KANA_WORDEXAM_MENU_NEXT, KANA_WORDEXAM_MENU_COUNT };
enum { KANA_WORDEXAM_RESULTS_DONE = 0, KANA_WORDEXAM_RESULTS_RETRY, KANA_WORDEXAM_RESULTS_COUNT };
enum { KANA_SCAN_MENU_BACK = 0, KANA_SCAN_MENU_CAMERA, KANA_SCAN_MENU_PHOTOS, KANA_SCAN_MENU_TRANSLATE, KANA_SCAN_MENU_WRITE, KANA_SCAN_MENU_COUNT };
enum { KANA_VIEWER_ADD_DONE = 0, KANA_VIEWER_ADD_TYPE, KANA_VIEWER_ADD_COUNT };
#define KANA_TOOLBAR_BACKDROP  (rde_color){ 0, 0, 0, 110 }
enum { KANA_PRACTICE_BACK = 0, KANA_PRACTICE_UNDO, KANA_PRACTICE_CLEAR, KANA_PRACTICE_SCORE, KANA_PRACTICE_FEWER, KANA_PRACTICE_MORE, KANA_PRACTICE_GUIDED, KANA_PRACTICE_COUNT };
enum { KANA_ALBUM_MENU_EXAMS = KANA_ALBUM_SORT_COUNT, KANA_ALBUM_MENU_PRACTICE, KANA_ALBUM_MENU_CLOSE, KANA_ALBUM_MENU_COUNT };   // the sorts first, in KANA_ALBUM_SORT_ order
RDE_INTERNAL void kana_toolbar_album_show_view(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_update_text_copy(kana_toolbar* _toolbar);
RDE_INTERNAL void kana_toolbar_update_translation(kana_toolbar* _toolbar);
enum { KANA_PRACTICE_SET_BACK = 0, KANA_PRACTICE_SET_UNDO, KANA_PRACTICE_SET_CLEAR, KANA_PRACTICE_SET_SCORE, KANA_PRACTICE_SET_GUIDED, KANA_PRACTICE_SET_NEXT, KANA_PRACTICE_SET_COUNT };
enum { KANA_PRACTICE_SUMMARY_AGAIN = 0, KANA_PRACTICE_SUMMARY_DONE, KANA_PRACTICE_SUMMARY_COUNT };

#define KANA_ALBUM_PRACTICE_MAX 10u   // the album's "Practice n": its weakest this many
enum { KANA_ALBUM_PAGE_BACK = 0, KANA_ALBUM_PAGE_PRACTICE, KANA_ALBUM_PAGE_COUNT };

// Browse's bar.
#define KANA_BROWSE_ROW_H     36.0f
#define KANA_BROWSE_FIELD_H   44.0f
#define KANA_BROWSE_SIDE_W    76.0f   // Draw, Clear
// Browse's chips (text.h); the JLPT levels are the same in every language.
static const KANA_TEXT_ KANA_FILTER_TEXTS[KANA_FILTER_COUNT] = { KANA_TEXT_ALL, KANA_TEXT_HIRAGANA, KANA_TEXT_KATAKANA, KANA_TEXT_KANJI, KANA_TEXT_COUNT, KANA_TEXT_COUNT,
                                                                 KANA_TEXT_COUNT, KANA_TEXT_COUNT, KANA_TEXT_COUNT, KANA_TEXT_STUDYING, KANA_TEXT_KNOWN };
static const c8* const  KANA_FILTER_LEVELS[KANA_FILTER_COUNT] = { NULL, NULL, NULL, NULL, "N5", "N4", "N3", "N2", "N1", NULL, NULL };
static const KANA_TEXT_ KANA_SORT_TEXTS[KANA_SORT_COUNT]     = { KANA_TEXT_SORT_DEFAULT, KANA_TEXT_SORT_STROKES, KANA_TEXT_SORT_ON, KANA_TEXT_SORT_KUN, KANA_TEXT_SORT_MEANING };

RDE_INTERNAL const c8* kana_toolbar_filter_label(u32 _i) {
    return KANA_FILTER_LEVELS[_i] != NULL ? KANA_FILTER_LEVELS[_i] : kana_text(KANA_FILTER_TEXTS[_i]);
}

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
    { 0xE010u, 128 },
    { 0xE036u, 128 },
    { 0xE038u,  64 },
    { 0xE058u, 128 },
    { 0xE06Cu, 128 },
    { 0xE094u, 128 },
    { 0xE0E4u, 160 },
    { 0xE0E6u,  64 },
    { 0xE0EAu, 224 },
    { 0xE0F8u,  96 },
    { 0xE10Au, 128 },
    { 0xE10Eu,  96 },
    { 0xE138u, 288 },
    { 0xE13Au, 352 },
    { 0xE150u,  96 },
    { 0xE156u,  96 },
    { 0xE182u, 128 },
    { 0xE184u,  96 },
    { 0xE18Au,  96 },
    { 0xE196u, 160 },
    { 0xE198u, 160 },
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
    { 0xE2A6u,  96 },
    { 0xE2CAu,  96 },
    { 0xE2CEu,  96 },
    { 0xE2D8u,  64 },
    { 0xE2DCu, 160 },
    { 0xE2F0u, 128 },
    { 0xE2F2u, 128 },
    { 0xE2F6u, 127 },
    { 0xE30Cu,  95 },
    { 0xE32Au, 128 },
    { 0xE34Cu, 128 },
    { 0xE39Cu,  96 },
    { 0xE39Eu, 160 },
    { 0xE3ACu, 128 },
    { 0xE3B4u, 128 },
    { 0xE3D0u, 256 },
    { 0xE3D4u, 128 },
    { 0xE3DCu,  64 },
    { 0xE3F6u,  96 },
    { 0xE422u,  96 },
    { 0xE432u,  96 },
    { 0xE444u, 160 },
    { 0xE44Au,  64 },
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
    { 0xE6EEu,  32 },
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
    { 0xEAF0u, 160 },
    { 0xEBB6u, 128 },
    { 0xEDC6u,  64 },
    { 0xEDF2u, 192 },
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
    rde_ui_button_set_text(_toolbar->brush_scale, kana_text(_toolbar->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_TEXT_TOOL_PAGE : KANA_TEXT_TOOL_SCREEN));
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
RDE_INTERNAL void kana_toolbar_layout(kana_toolbar* _toolbar);
rde_vec_2F kana_toolbar_virtual_size(const kana_toolbar* _toolbar);
rde_vec_2F kana_toolbar_clamp(const kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size);

RDE_INTERNAL b8 kana_toolbar_same_vec(rde_vec_2F _a, rde_vec_2F _b) {
    return memcmp(&_a, &_b, sizeof(rde_vec_2F)) == 0;
}

// A menu: a panel under the root with a row of buttons, each an icon over its
// label, hidden until shown. A button is as wide as its label needs (a label
// ending in a count, "Practice 0", with room for three digits). A NULL label is
// a button left out: its slot stays NULL, so the others keep their indices.
RDE_INTERNAL void kana_toolbar_menu_create(kana_toolbar* _toolbar, kana_toolbar_menu* _menu, rde_ui_node* _root,
                                           const c8* const* _labels, const c8* const* _icons, const rde_ui_event_callback* _callbacks, u32 _count) {
    _menu->count = _count < KANA_TOOLBAR_MENU_MAX ? _count : KANA_TOOLBAR_MENU_MAX;
    _menu->panel = rde_ui_image_create(NULL);
    kana_toolbar_style_panel(_menu->panel, KANA_TOOLBAR_ROW_RADIUS, 1.0f);

    rde_ui_node* _node = rde_ui_image_as_node(_menu->panel);
    rde_ui_node_set_blocks_input(_node, true);
    rde_ui_node_add_child(_root, _node);

    f32 _widths[KANA_TOOLBAR_MENU_MAX];
    u32 _shown = 0;
    for(u32 _i = 0; _i < _menu->count; _i++) {
        _shown += _labels[_i] != NULL ? 1u : 0u;
    }
    f32 _total = 2.0f * KANA_TOOLBAR_ROW_PADDING + KANA_TOOLBAR_ROW_SPACING * (f32)(_shown > 0 ? _shown - 1u : 0u);
    for(u32 _i = 0; _i < _menu->count; _i++) {
        if(_labels[_i] == NULL) {
            _widths[_i] = 0.0f;
            continue;
        }
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
        if(_labels[_i] == NULL) {
            _menu->buttons[_i] = NULL;
            continue;
        }
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

    // "Copied" goes back to "Copy" (and "Copy as text").
    if(_toolbar->copied_until > 0.0 && rde_engine_get_time_now() >= _toolbar->copied_until) {
        _toolbar->copied_until = 0.0;
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY], kana_text(KANA_TEXT_SEL_COPY));
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY_TEXT], kana_text(KANA_TEXT_SEL_COPY_TEXT));
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
    rde_ui_button_set_text(_study, kana_text(_mark == KANA_MARK_KNOWN ? KANA_TEXT_KNOWN : _mark == KANA_MARK_STUDYING ? KANA_TEXT_STUDYING : KANA_TEXT_STUDY));
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
    KANA_TEXTF(_label, KANA_TEXT_NEXT_N, KANA_TN(_planned));
    rde_ui_button_set_text(_toolbar->exam_setup_menu.buttons[KANA_EXAM_SETUP_NEXT], _label);
    kana_toolbar_set_enabled(_toolbar->exam_setup_menu.buttons[KANA_EXAM_SETUP_NEXT], _planned > 0);
    KANA_TEXTF(_label, KANA_TEXT_START_N, KANA_TN(_included));
    rde_ui_button_set_text(_toolbar->exam_preview_menu.buttons[KANA_EXAM_PREVIEW_START], _label);
    kana_toolbar_set_enabled(_toolbar->exam_preview_menu.buttons[KANA_EXAM_PREVIEW_START], _included > 0);
    rde_ui_button_set_text(_toolbar->exam_menu.buttons[KANA_EXAM_MENU_NEXT], kana_text(kana_exam_at_last(_exam) ? KANA_TEXT_FINISH : KANA_TEXT_NEXT));
    kana_toolbar_icon(_toolbar->exam_menu.buttons[KANA_EXAM_MENU_NEXT], kana_exam_at_last(_exam) ? KANA_ICON_FINISH : KANA_ICON_ARROW_RIGHT,
                      KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
    kana_toolbar_set_enabled(_toolbar->exam_results_menu.buttons[KANA_EXAM_RESULTS_RETRY], _nwrong > 0);
    kana_toolbar_set_enabled(_toolbar->exam_results_menu.buttons[KANA_EXAM_RESULTS_PRACTICE], _nwrong > 0);
}

RDE_INTERNAL void kana_toolbar_show_vocab(kana_toolbar* _toolbar, b8 _vocabing, b8 _wordexaming, rde_vec_2F _center);

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
    const b8 _scanning   = _toolbar->scan != NULL && _toolbar->scan->open;
    const b8 _wordexam_open = _toolbar->wordexam != NULL && _toolbar->wordexam->open;
    const b8 _vocab_open = _toolbar->vocab != NULL && _toolbar->vocab->open;
    const b8 _wordexaming = _wordexam_open && !_toolbar->viewer->open && !_practicing;
    const b8 _vocabing   = _vocab_open && !_wordexam_open && !_toolbar->viewer->open && !_practicing && !_stats_open;
    const b8 _under      = _toolbar->viewer->open || _practicing || _exam_open || _stats_open || _scanning || _wordexam_open || _vocab_open;   // something covers Browse / the chart / the album
    const b8 _browsing   = _toolbar->browse->open && !_under;
    const b8 _charting   = _toolbar->chart->open && !_under;
    const b8 _albuming   = _toolbar->album->open && !_under;
    const b8 _checking   = _toolbar->check->open && !_under;
    const b8 _full       = _practicing || _toolbar->viewer->open || _toolbar->browse->open || _toolbar->chart->open || _toolbar->album->open ||
                           _toolbar->check->open || _exam_open || _stats_open || _scanning || _wordexam_open || _vocab_open || _toolbar->covered;

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
    kana_wordcard_update(_toolbar);   // a word asked for (wordcard.h), opened; laid out again when the screen turns
    if(_viewing) {
        kana_toolbar_show_study(_toolbar);
    }
    kana_toolbar_show_exam(_toolbar, _examining, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->stats_menu, _statsing, _center);
    if(_toolbar->vocab != NULL && _toolbar->wordexam != NULL) {
        kana_toolbar_show_vocab(_toolbar, _vocabing, _wordexaming, _center);
    }
    // Text from a photo: Hold while the camera is live; then "Write n", as many
    // lines as are kept.
    if(_scanning) {
        const b8  _live = _toolbar->scan->stage == KANA_SCAN_LIVE;
        const u32 _kept = kana_scan_kept(_toolbar->scan);
        const u32 _for  = _live ? UINT32_MAX - 1u : _kept;
        if(_for != _toolbar->_scan_kept_shown) {
            _toolbar->_scan_kept_shown = _for;
            c8 _label[48];
            if(_live)          { snprintf(_label, sizeof(_label), "%s", kana_text(KANA_TEXT_SCAN_HOLD)); }
            else if(_kept > 0) { KANA_TEXTF(_label, KANA_TEXT_SCAN_WRITE_N, KANA_TN(_kept)); }
            else               { snprintf(_label, sizeof(_label), "%s", kana_text(KANA_TEXT_SCAN_WRITE)); }
            rde_ui_button_set_text(_toolbar->scan_menu.buttons[KANA_SCAN_MENU_WRITE], _label);
            kana_toolbar_icon(_toolbar->scan_menu.buttons[KANA_SCAN_MENU_WRITE], _live ? KANA_ICON_PAUSE : KANA_ICON_PEN, KANA_TOOLBAR_ICON_ABOVE,
                              KANA_TOOLBAR_ROW_ICON_PX);
            kana_toolbar_set_enabled(_toolbar->scan_menu.buttons[KANA_SCAN_MENU_WRITE], _live || _kept > 0);
        }
        // Translate with Google: chosen while the translations show.
        rde_ui_button* const _translate = _toolbar->scan_menu.buttons[KANA_SCAN_MENU_TRANSLATE];
        const u8             _on        = kana_scan_translating(_toolbar->scan) ? 1u : 0u;
        if(_translate != NULL && _on != _toolbar->_scan_translate_shown) {
            _toolbar->_scan_translate_shown = _on;
            if(_on) { kana_toolbar_button_selected(_translate); }
            else    { kana_toolbar_button_plain(_translate); }
            kana_toolbar_button_round(_translate, 12.0f);
            kana_toolbar_icon(_translate, KANA_ICON_TRANSLATE, KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
        }
    }
    kana_toolbar_menu_show(_toolbar, &_toolbar->scan_menu, _scanning, _center);
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
        KANA_TEXTF(_label, KANA_TEXT_PRACTICE_N, KANA_TN(_toolbar->_selected_shown));
        rde_ui_button_set_text(_toolbar->select_menu.buttons[KANA_SELECT_MENU_PRACTICE], _label);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_PRACTICE], _toolbar->_selected_shown > 0);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_NONE], _toolbar->_selected_shown > 0);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_SHEET], _toolbar->_selected_shown > 0);
        kana_toolbar_set_enabled(_toolbar->select_menu.buttons[KANA_SELECT_MENU_LIST], _toolbar->_selected_shown > 0);
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
        rde_ui_button_set_text(_toolbar->practice_set_menu.buttons[KANA_PRACTICE_SET_NEXT], kana_text(_finish ? KANA_TEXT_FINISH : KANA_TEXT_NEXT));
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
            KANA_TEXTF(_label, KANA_TEXT_PRACTICE_N, KANA_TN(_n));
            rde_ui_button_set_text(_toolbar->album_menu.buttons[KANA_ALBUM_MENU_PRACTICE], _n > 0 ? _label : kana_text(KANA_TEXT_PRACTICE));
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
    if(_albuming && _toolbar->_album_view_shown != (u32)_toolbar->album->view) {
        kana_toolbar_album_show_view(_toolbar);   // the view changed by something other than its menu
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
    kana_toolbar_update_text_copy(_toolbar);
    kana_toolbar_update_translation(_toolbar);

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
        { rde_ui_button_as_node(_toolbar->camera),      { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_image_as_node(_toolbar->separators[4]), _sep },
        { rde_ui_button_as_node(_toolbar->rotate),      { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->reset_view),  { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
    };
    // The camera only where text can be read from it (textscan.h): elsewhere it
    // and the hairline after it are left out of the bar, not shown dead.
    const b8 _camera = kana_textscan_available();
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->camera), _camera);
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->separators[4]), _camera);
    u32 _count = 0;
    for(u32 _i = 0; _i < sizeof(_items) / sizeof(_items[0]); _i++) {
        if(!_camera && (_items[_i].node == rde_ui_button_as_node(_toolbar->camera) || _items[_i].node == rde_ui_image_as_node(_toolbar->separators[4]))) {
            continue;
        }
        _items[_count++] = _items[_i];
    }

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
    // Asks only whether there is text (iOS says nothing): reading it is Paste text's.
    kana_toolbar_set_enabled(_toolbar->context_menu.buttons[KANA_CONTEXT_PASTE_TEXT], !rde_engine_is_clipboard_empty());
    kana_toolbar_set_enabled(_toolbar->context_menu.buttons[KANA_CONTEXT_SCAN], _toolbar->scan != NULL && kana_textscan_available());

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
    if(_tool != _toolbar->tool) {
        _toolbar->tool_before = _toolbar->tool;
    }
    _toolbar->tool = _tool;
    kana_toolbar_refresh(_toolbar);
}

void kana_toolbar_pen_double_tap(kana_toolbar* _toolbar, u8 _action) {
    switch((RDE_PEN_TAP_ACTION_)_action) {
        case RDE_PEN_TAP_ACTION_SWITCH_ERASER:
            // The eraser, and from it back to what was in hand.
            kana_toolbar_set_tool(_toolbar, _toolbar->tool != KANA_TOOL_ERASE ? KANA_TOOL_ERASE
                                          : _toolbar->tool_before != KANA_TOOL_ERASE ? _toolbar->tool_before : KANA_TOOL_DRAW);
            break;
        case RDE_PEN_TAP_ACTION_SWITCH_PREVIOUS:
            kana_toolbar_set_tool(_toolbar, _toolbar->tool_before);
            break;
        case RDE_PEN_TAP_ACTION_SHOW_COLOR_PALETTE:
        case RDE_PEN_TAP_ACTION_SHOW_INK_ATTRIBUTES:
        case RDE_PEN_TAP_ACTION_SHOW_CONTEXTUAL_PALETTE:
            kana_toolbar_set_palette_open(_toolbar, !_toolbar->palette_open);   // the colours and the brush
            break;
        default:
            break;   // off, or the system's own shortcut
    }
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
    rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY], kana_text(KANA_TEXT_SEL_COPIED));
    _toolbar->copied_until = rde_engine_get_time_now() + KANA_TOOLBAR_COPIED_TIME;
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A line for the page (kana.c shows it a moment).
void kana_toolbar_notice(kana_toolbar* _toolbar, const c8* _text) {
    snprintf(_toolbar->notice, sizeof(_toolbar->notice), "%s", _text);
    _toolbar->notice_at = rde_engine_get_time_now();
}

// The selection read (textink.h), for the clipboard or the translator: false
// when a reading cannot start now (one under way).
RDE_INTERNAL b8 kana_toolbar_read_selection(kana_toolbar* _toolbar, b8 _for_translate) {
    if(!_toolbar->_text_reader_ready) {
        kana_textink_reader_init(&_toolbar->text_reader, _toolbar->browse->db, &_toolbar->browse->catalog);
        _toolbar->_text_reader_ready = true;
    }
    if(!kana_textink_read(&_toolbar->text_reader, _toolbar->ink, (const u32*)_toolbar->lasso->selected.memory, kana_lasso_count(_toolbar->lasso))) {
        return false;
    }
    _toolbar->text_for_translate = _for_translate;
    _toolbar->text_for_vocab     = false;
    return true;
}

// Save word: the selection read (written by hand, or pasted text), then the word
// card with what it says (wordcard.h: a word of the dictionary, or the words in it).
void kana_toolbar_save_selection(kana_toolbar* _toolbar) {
    if(kana_lasso_count(_toolbar->lasso) > 0 && kana_toolbar_read_selection(_toolbar, false)) {
        _toolbar->text_for_vocab = true;
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_SAVE_WORD], kana_text(KANA_TEXT_SEL_READING));
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_save_word(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_save_selection((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Copy as text: the selection read; the text goes to the system clipboard when
// the reading is done (kana_toolbar_update_text_copy).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_copy_text(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(kana_toolbar_read_selection(_toolbar, false)) {
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY_TEXT], kana_text(KANA_TEXT_SEL_READING));
        _toolbar->copied_until = 0.0;
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- Translate with Google on a selection -------------------------------------------------

// Which selection: its strokes, as one number (a new lasso, or strokes added or
// taken away, gives another). The card is for the one it was read from.
RDE_INTERNAL u32 kana_toolbar_selection_key(const kana_toolbar* _toolbar) {
    const u32  _n   = kana_lasso_count(_toolbar->lasso);
    const u32* _ids = (const u32*)_toolbar->lasso->selected.memory;
    u32        _key = 2166136261u ^ _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _key = (_key ^ _ids[_i]) * 16777619u;
    }
    return _n == 0 ? 0u : (_key | 1u);
}

void kana_toolbar_translate_selection(kana_toolbar* _toolbar) {
    if(kana_lasso_count(_toolbar->lasso) == 0 || !kana_translate_available()) {
        return;
    }
    if(!kana_mlkit_enabled()) {
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_SEL_TRANSLATE_MLKIT_OFF));
        return;
    }
    if(kana_toolbar_read_selection(_toolbar, true)) {
        _toolbar->translation_state    = KANA_TOOLBAR_TRANSLATION_READING;
        _toolbar->translation_of       = kana_toolbar_selection_key(_toolbar);
        _toolbar->translation_prepared   = false;
        _toolbar->translation_from[0]    = 0;
        _toolbar->translation[0]         = 0;
        _toolbar->translation_word_count = 0;
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_translate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar_translate_selection((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

void kana_toolbar_render_translation(kana_toolbar* _toolbar, rde_window* _window) {
    rde_vec_2F _min, _max;
    if(_toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_NONE || _toolbar->font == NULL ||
       !kana_lasso_bounds(_toolbar->lasso, _toolbar->ink, &_min, &_max)) {
        _toolbar->translation_min = _toolbar->translation_max = (rde_vec_2F){ 0.0f, 0.0f };
        return;
    }
    const kana_theme* _t       = kana_theme_active();
    rde_font* const   _font    = _toolbar->font;
    const f32         _font_px = (f32)KANA_TOOLBAR_FONT_SIZE;
    const rde_vec_2I  _size    = rde_window_get_size(_window);
    const rde_vec_4I  _insets  = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sl      = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_TOOLBAR_SCREEN_EDGE;
    const f32         _sr      = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_TOOLBAR_SCREEN_EDGE;
    const f32         _st      = (f32)_size.y * 0.5f - (f32)_insets.y - KANA_TOOLBAR_SCREEN_EDGE;
    const f32         _sb      = -(f32)_size.y * 0.5f + (f32)_insets.w + KANA_TOOLBAR_SCREEN_EDGE;

    // What it says: what was read, and its translation — or how far it got.
    const c8* _from = _toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_READING ? kana_text(KANA_TEXT_SEL_READING) : _toolbar->translation_from;
    const c8* _to   = NULL;
    rde_color _to_c = _t->text_soft;
    switch(_toolbar->translation_state) {
        case KANA_TOOLBAR_TRANSLATION_GETTING:
            _to = kana_text(kana_translate_state(kana_translate_target()) == KANA_TRANSLATE_DOWNLOADING ? KANA_TEXT_SCAN_TRANSLATE_GETTING : KANA_TEXT_SCAN_TRANSLATING);
            break;
        case KANA_TOOLBAR_TRANSLATION_ASKED:  _to = kana_text(KANA_TEXT_SCAN_TRANSLATING); break;
        case KANA_TOOLBAR_TRANSLATION_FAILED: _to = kana_text(KANA_TEXT_SCAN_TRANSLATE_FAILED); _to_c = _t->score_poor; break;
        case KANA_TOOLBAR_TRANSLATION_DONE:
            _to   = _toolbar->translation[0] != 0 ? _toolbar->translation : kana_text(KANA_TEXT_SEL_TRANSLATION_NONE);
            _to_c = _toolbar->translation[0] != 0 ? _t->text : _t->text_soft;
            break;
        default: break;
    }

    // Its size: as wide as it may be, as tall as its lines.
    const f32 _w      = fminf(KANA_TOOLBAR_CARD_W, _sr - _sl);
    const f32 _inner  = _w - 2.0f * KANA_TOOLBAR_CARD_PAD;
    const u32 _from_n = kana_draw_text_wrap_lines(_font, _font_px, _from, KANA_TOOLBAR_CARD_FROM, _inner);
    const u32 _to_n   = _to != NULL ? kana_draw_text_wrap_lines(_font, _font_px, _to, KANA_TOOLBAR_CARD_TO, _inner) : 0u;
    const u32 _words  = _toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_READING ? _toolbar->translation_word_count : 0u;
    const f32 _h      = 2.0f * KANA_TOOLBAR_CARD_PAD + KANA_TRANSLATE_BADGE_H + 10.0f + (f32)_from_n * KANA_TOOLBAR_CARD_FROM_L +
                        (_to_n > 0 ? 6.0f + (f32)_to_n * KANA_TOOLBAR_CARD_TO_L : 0.0f) + (_words > 0 ? 8.0f + (f32)_words * KANA_TOOLBAR_CARD_WORD : 0.0f);

    // Where: on the selection's other side from its menu (the menu goes above
    // when there is room), centred on it, kept on screen.
    const kana_view* _view    = &_toolbar->view->view;
    const f32        _box_t   = _max.y * _view->zoom + _view->offset.y + KANA_LASSO_BOX_PAD;
    const f32        _box_b   = _min.y * _view->zoom + _view->offset.y - KANA_LASSO_BOX_PAD;
    const f32        _cx      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x;
    const b8         _menu_up = _box_t + KANA_TOOLBAR_MENU_GAP + _toolbar->selection_menu.size.y <= _st;
    f32              _top     = _menu_up ? _box_b - KANA_TOOLBAR_MENU_GAP : _box_t + KANA_TOOLBAR_MENU_GAP + _h;
    _top = fmaxf(fminf(_top, _st), _sb + _h);
    const f32 _left = fmaxf(fminf(_cx - _w * 0.5f, _sr - _w), _sl);
    _toolbar->translation_min = (rde_vec_2F){ _left, _top - _h };
    _toolbar->translation_max = (rde_vec_2F){ _left + _w, _top };

    kana_draw_card(_toolbar->translation_min, _toolbar->translation_max, 16.0f, _t->surface, _t->outline);
    const rde_color _s = _t->surface;
    f32             _y = _top - KANA_TOOLBAR_CARD_PAD - KANA_TRANSLATE_BADGE_H * 0.5f;
    // With a voice and something read: a speaker at the top right says it.
    if(kana_speech_available() && _toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_READING) {
        const rde_vec_2F _c = { _left + _w - KANA_TOOLBAR_CARD_PAD - 10.0f, _y - 6.0f };
        kana_draw_icon(_font, _font_px, KANA_ICON_SPEAK, _c, 20.0f, _t->accent);
        _toolbar->translation_speak_min = (rde_vec_2F){ _c.x - 26.0f, _c.y - 22.0f };
        _toolbar->translation_speak_max = (rde_vec_2F){ _c.x + 22.0f, _c.y + 22.0f };
    } else {
        _toolbar->translation_speak_min = _toolbar->translation_speak_max = (rde_vec_2F){ 0.0f, 0.0f };
    }
    kana_translate_draw_badge(_left + KANA_TOOLBAR_CARD_PAD, _y, 0.299f * (f32)_s.r + 0.587f * (f32)_s.g + 0.114f * (f32)_s.b < 128.0f);
    _y -= KANA_TRANSLATE_BADGE_H * 0.5f + 10.0f;
    kana_draw_text_wrap(_font, _font_px, _from, _left + KANA_TOOLBAR_CARD_PAD, _y - 16.0f, KANA_TOOLBAR_CARD_FROM, _inner, KANA_TOOLBAR_CARD_FROM_L, _t->text_soft);
    _y -= (f32)_from_n * KANA_TOOLBAR_CARD_FROM_L + 6.0f;
    if(_to != NULL) {
        kana_draw_text_wrap(_font, _font_px, _to, _left + KANA_TOOLBAR_CARD_PAD, _y - 18.0f, KANA_TOOLBAR_CARD_TO, _inner, KANA_TOOLBAR_CARD_TO_L, _to_c);
        _y -= (f32)_to_n * KANA_TOOLBAR_CARD_TO_L;
    }
    // Its words: + to add (✓ the learner's), the word, its reading, its meaning.
    _y -= 8.0f;
    _toolbar->translation_words_top   = _y;
    _toolbar->translation_words_left  = _left;
    _toolbar->translation_words_right = _left + _w;
    const kana_kanji_db* _db = _toolbar->browse != NULL ? _toolbar->browse->db : NULL;
    for(u32 _k = 0; _k < _words && _db != NULL; _k++) {
        kana_kanji_word _word;
        if(!kana_kanji_word_at(_db, _toolbar->translation_words[_k], &_word)) {
            continue;
        }
        const f32 _mid    = _y - ((f32)_k + 0.5f) * KANA_TOOLBAR_CARD_WORD;
        const b8  _theirs = kana_vocab_find(_word.written, _word.reading) != 0u;
        if(_theirs) {
            kana_draw_icon_fill(KANA_ICON_BOOKMARK, (rde_vec_2F){ _left + KANA_TOOLBAR_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        } else {
            kana_draw_icon(_font, _font_px, KANA_ICON_BOOKMARK, (rde_vec_2F){ _left + KANA_TOOLBAR_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        }
        c8 _say[400];
        snprintf(_say, sizeof(_say), "%s  %s  ·  %s", _word.written, _word.reading, _word.meaning);
        const f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _say, 14.0f, _inner - 28.0f, 0.55f);
        kana_draw_text(_font, _font_px, _say, _left + KANA_TOOLBAR_CARD_PAD + 28.0f, _mid - _px * 0.36f, _px, _theirs ? _t->text : _t->text_soft);
    }
}

b8 kana_toolbar_card_press(kana_toolbar* _toolbar, rde_vec_2F _screen) {
    // A word's row: the word card (wordcard.h), to save it or see it saved.
    const kana_kanji_db* _db = _toolbar->browse != NULL ? _toolbar->browse->db : NULL;
    if(_toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_NONE && _toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_READING &&
       _db != NULL && _screen.x >= _toolbar->translation_words_left && _screen.x <= _toolbar->translation_words_right &&
       _screen.y <= _toolbar->translation_words_top) {
        const i32 _k = (i32)floorf((_toolbar->translation_words_top - _screen.y) / KANA_TOOLBAR_CARD_WORD);
        kana_kanji_word _w;
        if(_k >= 0 && (u32)_k < _toolbar->translation_word_count && kana_kanji_word_at(_db, _toolbar->translation_words[_k], &_w)) {
            kana_wordcard_ask(_w.written, _w.reading, _w.meaning, 0u);
            return true;
        }
    }
    if(_toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_NONE || _toolbar->translation_from[0] == 0 ||
       _screen.x < _toolbar->translation_speak_min.x || _screen.x > _toolbar->translation_speak_max.x ||
       _screen.y < _toolbar->translation_speak_min.y || _screen.y > _toolbar->translation_speak_max.y) {
        return false;
    }
    kana_speak(_toolbar->translation_from);
    return true;
}

// Once a frame: the card goes with its selection; otherwise the next step —
// the models asked for (the first time), the text sent, the answer taken.
RDE_INTERNAL void kana_toolbar_update_translation(kana_toolbar* _toolbar) {
    if(_toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_NONE) {
        return;
    }
    if(kana_toolbar_selection_key(_toolbar) != _toolbar->translation_of) {
        _toolbar->translation_state = KANA_TOOLBAR_TRANSLATION_NONE;
        return;
    }
    const c8* _to = kana_translate_target();
    if(_toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_GETTING) {
        const KANA_TRANSLATE_STATE_ _state = kana_translate_state(_to);
        if(_state == KANA_TRANSLATE_READY) {
            _toolbar->translation_ticket = kana_translate_text(_toolbar->translation_from, _to);
            _toolbar->translation_state  = _toolbar->translation_ticket != 0u ? KANA_TOOLBAR_TRANSLATION_ASKED : KANA_TOOLBAR_TRANSLATION_DONE;
        } else if(_state == KANA_TRANSLATE_MISSING || (_state == KANA_TRANSLATE_FAILED && !_toolbar->translation_prepared)) {
            if(!_toolbar->translation_prepared) {
                _toolbar->translation_prepared = true;
                kana_translate_prepare(_to);
            }
        } else if(_state == KANA_TRANSLATE_FAILED || _state == KANA_TRANSLATE_UNAVAILABLE) {
            _toolbar->translation_state = KANA_TOOLBAR_TRANSLATION_FAILED;
        }
    }
    if(_toolbar->translation_state == KANA_TOOLBAR_TRANSLATION_ASKED) {
        u32 _ticket;
        c8  _answer[KANA_TRANSLATE_TEXT];
        while(kana_translate_poll(&_ticket, _answer, sizeof(_answer))) {
            if(_ticket == _toolbar->translation_ticket) {
                memcpy(_toolbar->translation, _answer, sizeof(_toolbar->translation));
                _toolbar->translation_state = KANA_TOOLBAR_TRANSLATION_DONE;
            }
        }
    }
}

// Once a frame: a reading done goes to the clipboard, and the page says what.
RDE_INTERNAL void kana_toolbar_update_text_copy(kana_toolbar* _toolbar) {
    if(!_toolbar->_text_reader_ready || !kana_textink_update(&_toolbar->text_reader)) {
        return;
    }
    const c8* _text = _toolbar->text_reader.text;
    c8        _line[256];
    if(_toolbar->text_for_vocab) {
        // For Save word: what was read, into the word card.
        _toolbar->text_for_vocab = false;
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_SAVE_WORD], kana_text(KANA_TEXT_SEL_SAVE_WORD));
        if(_text[0] == 0) {
            kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_NOTICE_NOTHING_READ));
            return;
        }
        kana_wordcard_ask_read(_toolbar->browse != NULL ? _toolbar->browse->db : NULL, _text);
        return;
    }
    if(_toolbar->text_for_translate) {
        // For Translate with Google: what was read goes to the translator.
        _toolbar->text_for_translate = false;
        if(_toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_READING) {
            return;   // the selection went since
        }
        if(_text[0] == 0) {
            _toolbar->translation_state = KANA_TOOLBAR_TRANSLATION_NONE;
            kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_NOTICE_NOTHING_READ));
            return;
        }
        snprintf(_toolbar->translation_from, sizeof(_toolbar->translation_from), "%s", _text);
        _toolbar->translation_state      = KANA_TOOLBAR_TRANSLATION_GETTING;
        _toolbar->translation_word_count = _toolbar->browse != NULL && _toolbar->browse->db != NULL
                                         ? kana_wordsplit(_toolbar->browse->db, _text, _toolbar->translation_words, KANA_WORDSPLIT_MAX) : 0u;
        return;
    }
    if(_text[0] == 0) {
        rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY_TEXT], kana_text(KANA_TEXT_SEL_COPY_TEXT));
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_NOTICE_NOTHING_READ));
        return;
    }
    rde_engine_set_clipboard(_text);
    rde_ui_button_set_text(_toolbar->selection_menu.buttons[KANA_SELECTION_COPY_TEXT], kana_text(KANA_TEXT_SEL_COPIED));
    _toolbar->copied_until = rde_engine_get_time_now() + KANA_TOOLBAR_COPIED_TIME;

    // What was copied, when it is short (on one line); otherwise how much.
    u32       _chars = 0;
    c8        _one_line[sizeof(_toolbar->text_reader.text)];
    usize     _n     = 0;
    const c8* _p     = _text;
    for(;;) {
        const c8* _from = _p;
        const u32 _cp   = kana_kanji_utf8_next(&_p);
        if(_cp == 0) {
            break;
        }
        _chars++;
        if(_cp == '\n') {
            _one_line[_n++] = ' ';
        } else {
            memcpy(&_one_line[_n], _from, (usize)(_p - _from));
            _n += (usize)(_p - _from);
        }
    }
    _one_line[_n] = 0;
    if(_chars <= KANA_TOOLBAR_NOTICE_CHARS) {
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_COPIED_TEXT, KANA_TS(_one_line));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_COPIED_N, KANA_TN(_chars));
    }
    kana_toolbar_notice(_toolbar, _line);
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

void kana_toolbar_paste_text(kana_toolbar* _toolbar, const c8* _text, rde_vec_2F _canvas) {
    const f32 _zoom  = _toolbar->view->view.zoom > 0.0f ? _toolbar->view->view.zoom : 1.0f;
    const f32 _width = (f32)rde_window_get_size(_toolbar->window).x * KANA_TOOLBAR_TEXT_WIDTH;
    const kana_textink_result _r = kana_textink_write(_toolbar->browse->db, _text, KANA_TOOLBAR_TEXT_CELL / _zoom, _width / _zoom,
                                                     kana_ink_pen_radius(_toolbar->ink, 0.5f), _toolbar->ink->color, &_toolbar->_text_clip);
    if(_r.drawn == 0) {
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_NOTICE_NOTHING_TO_WRITE));
        return;
    }
    // What was written comes in selected, ready to drag: the Lasso's job, as Paste.
    kana_toolbar_set_tool(_toolbar, KANA_TOOL_LASSO);
    kana_lasso_paste_clip(_toolbar->lasso, _toolbar->ink, &_toolbar->_text_clip, _canvas);
    if(_r.skipped > 0) {
        c8 _line[192];
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_LEFT_OUT, KANA_TN(_r.skipped));
        kana_toolbar_notice(_toolbar, _line);
    }
    kana_toolbar_update(_toolbar);
}

// Paste text: the system clipboard's text, where the menu was opened.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_paste_text(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_close_context_menu(_toolbar);
    c8* _text = rde_engine_get_clipboard();
    if(_text == NULL) {
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_NOTICE_NO_TEXT));
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    kana_toolbar_paste_text(_toolbar, _text, _toolbar->context_canvas);
    rde_engine_free_clipboard_str_ptr(_text);
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

// The bar's camera: Text from a photo with the camera live straight away; what
// is written goes to the middle of the page as it is on screen.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_camera(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->scan == NULL) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    kana_toolbar_close_context_menu(_toolbar);
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_set_paper_open(_toolbar, false);
    kana_scan_open(_toolbar->scan, _toolbar->window, kana_canvas_from_screen(_toolbar->view, (rde_vec_2F){ 0.0f, 0.0f }));
    kana_scan_camera(_toolbar->scan);
    _toolbar->_scan_kept_shown = UINT32_MAX;
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_reset_view(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_canvas_reset_view(((kana_toolbar*)_user_data)->view);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}




// --- the album's rows ----------------------------------------------------------------

// The album's sorts and Exams, one of them selected: the order shown, or the exams.
RDE_INTERNAL void kana_toolbar_album_show_view(kana_toolbar* _toolbar) {
    const u32 _shown = _toolbar->album->view == KANA_ALBUM_VIEW_EXAMS ? (u32)KANA_ALBUM_MENU_EXAMS : (u32)_toolbar->album->sort;
    _toolbar->_album_view_shown = (u32)_toolbar->album->view;
    for(u32 _i = 0; _i <= (u32)KANA_ALBUM_MENU_EXAMS; _i++) {
        if(_i == _shown) { kana_toolbar_button_selected(_toolbar->album_menu.buttons[_i]); }
        else             { kana_toolbar_button_quiet(_toolbar->album_menu.buttons[_i]); }
        kana_toolbar_button_round(_toolbar->album_menu.buttons[_i], 12.0f);
    }
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_sort(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_chip_ref* _ref = (const kana_toolbar_chip_ref*)_user_data;
    kana_toolbar* _toolbar = _ref->toolbar;
    kana_album_set_sort(_toolbar->album, (KANA_ALBUM_SORT_)_ref->index);
    kana_toolbar_album_show_view(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_album_exams(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_album_set_view(_toolbar->album, KANA_ALBUM_VIEW_EXAMS);
    kana_toolbar_album_show_view(_toolbar);
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

// --- the viewer's Add -----------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_add_done(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_viewer_set_adding(_toolbar->viewer, false);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_add_type(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordcard_ask_typed(kana_viewer_codepoint(((kana_toolbar*)_user_data)->viewer));
    kana_toolbar_update((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// --- exams ----------------------------------------------------------------------------

// --- text from a photo ------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_toolbar_close_context_menu(_toolbar);
    kana_scan_open(_toolbar->scan, _toolbar->window, _toolbar->context_canvas);   // its lines go where the menu was opened
    _toolbar->_scan_kept_shown = UINT32_MAX;
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_scan_close(_toolbar->scan);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan_camera(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_scan_camera(((kana_toolbar*)_user_data)->scan);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan_photos(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_scan_photos(((kana_toolbar*)_user_data)->scan);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Translate with Google, on or off: the panel of translations (scan.h).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan_translate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_scan_translate(_toolbar->scan);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Live: Hold. Otherwise Write — the owner writes them on the page (kana_scan_take_text).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_scan_write(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_scan* _scan = ((kana_toolbar*)_user_data)->scan;
    if(_scan->stage == KANA_SCAN_LIVE) {
        kana_scan_hold(_scan);
    } else {
        kana_scan_write(_scan);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

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

// A practice sheet of the character on screen (sheet.h): made by the owner.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_viewer_sheet(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar*      _toolbar = (kana_toolbar*)_user_data;
    const kana_viewer* _viewer  = _toolbar->viewer;
    if(rde_arr_length(&_viewer->list) > 0) {
        _toolbar->_sheet_one   = ((const u32*)_viewer->list.memory)[_viewer->position];
        _toolbar->_sheet_asked = KANA_TOOLBAR_SHEET_VIEWER;
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

u32 kana_toolbar_take_sheet(kana_toolbar* _toolbar, const u32** _records) {
    const u8 _asked = _toolbar->_sheet_asked;
    _toolbar->_sheet_asked = KANA_TOOLBAR_SHEET_NONE;
    if(_asked == KANA_TOOLBAR_SHEET_VIEWER) {
        *_records = &_toolbar->_sheet_one;
        return 1u;
    }
    if(_asked == KANA_TOOLBAR_SHEET_SELECTION && _toolbar->selection != NULL) {
        *_records = kana_selection_records(_toolbar->selection);
        return kana_selection_count(_toolbar->selection);
    }
    if(_asked == KANA_TOOLBAR_SHEET_VOCAB && _toolbar->vocab != NULL) {
        *_records = _toolbar->_sheet_records;
        return kana_vocabview_records(_toolbar->vocab, _toolbar->_sheet_records, KANA_SHEET_RECORDS);
    }
    *_records = NULL;
    return 0u;
}

// --- the Vocabulary screen and word exams ------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_vocabview_close(_toolbar->vocab);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// + Word: typed in (the list shown, ticked).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_word(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_wordcard_prefer_list(_toolbar->vocab->list);
    kana_wordcard_ask_typed(0u);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// What the screen shows is called: the list's name, or Vocabulary.
RDE_INTERNAL const c8* kana_toolbar_vocab_title(const kana_toolbar* _toolbar) {
    return _toolbar->vocab->list != 0u ? kana_vocab_list_name(_toolbar->vocab->list) : kana_text(KANA_TEXT_VOCAB);
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_review(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    u32           _due[KANA_REVIEW_SESSION];
    const u32     _n = kana_vocabview_due(_toolbar->vocab, _due, KANA_REVIEW_SESSION);
    if(_n == 0u) {
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_REVIEWS_NONE));
    } else {
        kana_wordexam_open_review(_toolbar->wordexam, _due, _n);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_exam(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    const u32*    _ids;
    const u32     _n = kana_vocabview_words(_toolbar->vocab, &_ids);
    kana_wordexam_open(_toolbar->wordexam, _ids, _n, kana_toolbar_vocab_title(_toolbar));
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_sheet(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->_sheet_asked = KANA_TOOLBAR_SHEET_VOCAB;
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Practice: the words' characters, as a set.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_vocab_practice(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    const u32     _n       = kana_vocabview_records(_toolbar->vocab, _toolbar->_sheet_records, KANA_SHEET_RECORDS);
    if(_n > 0u) {
        kana_practice_open_set(_toolbar->practice, _toolbar->_sheet_records, _n);
    }
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_close(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_wordexam_close(_toolbar->wordexam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_start(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_wordexam_start(_toolbar->wordexam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_undo(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordexam_undo(((kana_toolbar*)_user_data)->wordexam);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordexam_clear(((kana_toolbar*)_user_data)->wordexam);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_next(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_wordexam_next(_toolbar->wordexam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_wordexam_retry(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    kana_wordexam_retry_wrong(_toolbar->wordexam);
    kana_toolbar_update(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The Vocabulary screen's row and a word exam's, for what is open; their labels
// (Review n, Start n, Next / Finish) when what they count changes.
RDE_INTERNAL void kana_toolbar_show_vocab(kana_toolbar* _toolbar, b8 _vocabing, b8 _wordexaming, rde_vec_2F _center) {
    kana_wordexam* _exam   = _toolbar->wordexam;
    const b8       _setup  = _wordexaming && _exam->stage == KANA_WORDEXAM_SETUP;
    const b8       _write  = _wordexaming && _exam->stage == KANA_WORDEXAM_WRITING;
    const b8       _result = _wordexaming && _exam->stage == KANA_WORDEXAM_RESULTS;
    kana_toolbar_menu_show(_toolbar, &_toolbar->vocab_menu, _vocabing, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->wordexam_setup_menu, _setup, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->wordexam_menu, _write, _center);
    kana_toolbar_menu_show(_toolbar, &_toolbar->wordexam_results_menu, _result, _center);
    c8 _label[48];
    if(_vocabing) {
        const u32* _ids;
        const u32  _words = kana_vocabview_words(_toolbar->vocab, &_ids);
        const u32  _for   = kana_vocab_revision() * 131u + kana_reviews_revision() * 17u + kana_reviews_today() * 7u + _toolbar->vocab->list;
        if(_for != _toolbar->_vocab_shown) {
            _toolbar->_vocab_shown = _for;
            u32       _due[KANA_REVIEW_SESSION];
            const u32 _n = kana_vocabview_due(_toolbar->vocab, _due, KANA_REVIEW_SESSION);
            KANA_TEXTF(_label, KANA_TEXT_VOCAB_REVIEW_N, KANA_TN(_n));
            rde_ui_button_set_text(_toolbar->vocab_menu.buttons[KANA_VOCAB_MENU_REVIEW], _label);
            kana_toolbar_set_enabled(_toolbar->vocab_menu.buttons[KANA_VOCAB_MENU_REVIEW], _n > 0u);
            for(u32 _b = KANA_VOCAB_MENU_EXAM; _b <= KANA_VOCAB_MENU_PRACTICE; _b++) {
                kana_toolbar_set_enabled(_toolbar->vocab_menu.buttons[_b], _words > 0u);
            }
        }
    }
    if(!_wordexaming) {
        _toolbar->_wordexam_shown = UINT32_MAX;
        return;
    }
    const u32 _planned = _setup ? kana_wordexam_planned(_exam) : 0u;
    const u32 _state   = (u32)_exam->stage | (_planned << 2) | ((u32)kana_wordexam_at_last(_exam) << 20) | ((u32)kana_wordexam_graded(_exam) << 21) |
                         (kana_wordexam_wrong(_exam) << 22);
    if(_state == _toolbar->_wordexam_shown) {
        return;
    }
    _toolbar->_wordexam_shown = _state;
    KANA_TEXTF(_label, KANA_TEXT_START_N, KANA_TN(_planned));
    rde_ui_button_set_text(_toolbar->wordexam_setup_menu.buttons[KANA_WORDEXAM_SETUP_START], _label);
    kana_toolbar_set_enabled(_toolbar->wordexam_setup_menu.buttons[KANA_WORDEXAM_SETUP_START], _planned > 0u);
    rde_ui_button_set_text(_toolbar->wordexam_menu.buttons[KANA_WORDEXAM_MENU_NEXT], kana_text(kana_wordexam_at_last(_exam) ? KANA_TEXT_FINISH : KANA_TEXT_NEXT));
    kana_toolbar_icon(_toolbar->wordexam_menu.buttons[KANA_WORDEXAM_MENU_NEXT], kana_wordexam_at_last(_exam) ? KANA_ICON_FINISH : KANA_ICON_ARROW_RIGHT,
                      KANA_TOOLBAR_ICON_ABOVE, KANA_TOOLBAR_ROW_ICON_PX);
    kana_toolbar_set_enabled(_toolbar->wordexam_results_menu.buttons[KANA_WORDEXAM_RESULTS_RETRY], kana_wordexam_graded(_exam) && kana_wordexam_wrong(_exam) > 0u);
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

// Sheet: the ticked as a practice sheet (sheet.h), in the order ticked: made by the owner.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_sheet(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection != NULL && kana_selection_count(_toolbar->selection) > 0) {
        _toolbar->_sheet_asked = KANA_TOOLBAR_SHEET_SELECTION;
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// Save list: the ticked, in the order ticked, as a new list of the vocabulary
// (vocab.h): each character a word — written as itself, read as its first kun
// reading (else on; a kana, itself), its meanings (a kana's, its romaji).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_ticks_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->selection == NULL || kana_selection_count(_toolbar->selection) == 0 || _toolbar->browse->db == NULL) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    c8 _name[KANA_VOCAB_LIST_NAME];
    kana_vocab_list_new_name(_name, sizeof(_name));
    const u32 _list = kana_vocab_list_add(_name);
    if(_list == 0u) {
        kana_toolbar_notice(_toolbar, kana_text(KANA_TEXT_VOCAB_LISTS_FULL));
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    const kana_kanji_db* _db = _toolbar->browse->db;
    u32                  _n  = 0;
    for(u32 _i = 0; _i < kana_selection_count(_toolbar->selection); _i++) {
        kana_kanji_info _ch;
        if(!kana_kanji_at(_db, kana_selection_records(_toolbar->selection)[_i], &_ch)) {
            continue;
        }
        c8 _written[8];
        kana_kanji_utf8(_ch.codepoint, _written);
        c8        _reading[KANA_USERWORD_READING] = "";
        const c8* _meaning = kana_kanji_meanings(_db, &_ch);
        const c8* _romaji  = kana_chart_romaji(_ch.codepoint);
        if(_romaji != NULL && _ch.text == UINT32_MAX) {
            snprintf(_reading, sizeof(_reading), "%s", _written);   // a kana
            _meaning = _romaji;
        } else {
            // The first reading: kun (up to its okurigana mark) else on, without - marks.
            const c8* _from = kana_kanji_kun(_db, &_ch)[0] != 0 ? kana_kanji_kun(_db, &_ch) : kana_kanji_on(_db, &_ch);
            usize     _k    = 0;
            for(const c8* _c = _from; *_c != 0 && *_c != '.' && _k + 1u < sizeof(_reading);) {
                if(_c[0] == '\xE3' && _c[1] == '\x80' && _c[2] == '\x81') {
                    break;   // 、 the next reading
                }
                if(*_c != '-') {
                    _reading[_k++] = *_c;
                }
                _c++;
            }
            _reading[_k] = 0;
        }
        const u32 _id = kana_vocab_add(_written, _reading, _meaning, _ch.codepoint);
        if(_id != 0u) {
            kana_vocab_set_in_list(_list, _id, true);
            _n++;
        }
    }
    c8 _line[192];
    KANA_TEXTF(_line, KANA_TEXT_VOCAB_LIST_SAVED, KANA_TN(_n), KANA_TS(_name));
    kana_toolbar_notice(_toolbar, _line);
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
    const c8* _filters[KANA_FILTER_COUNT];
    const c8* _sorts[KANA_SORT_COUNT];
    for(u32 _i = 0; _i < KANA_FILTER_COUNT; _i++) { _filters[_i] = kana_toolbar_filter_label(_i); }
    for(u32 _i = 0; _i < KANA_SORT_COUNT; _i++)   { _sorts[_i]   = kana_text(KANA_SORT_TEXTS[_i]); }
    kana_toolbar_chip_row(_toolbar, _toolbar->filter_chips, _filters, KANA_FILTER_COUNT, _left, _width, _y);
    _y -= KANA_BROWSE_ROW_H + _gap;
    kana_toolbar_chip_row(_toolbar, _toolbar->sort_chips, _sorts, KANA_SORT_COUNT, _left, _width, _y);

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
    kana_wordcard_apply_theme(_toolbar);
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
        _toolbar->brush_scale, _toolbar->paper, _toolbar->rotate, _toolbar->reset_view, _toolbar->camera,
        _toolbar->paper_choices[0], _toolbar->paper_choices[1], _toolbar->paper_choices[2], _toolbar->paper_choices[3],
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        kana_toolbar_restyle_quiet(_tools[_i]);
    }
    rde_ui_button* const _plain[] = { _toolbar->draw_toggle, _toolbar->parts_toggle, _toolbar->pad_clear };
    for(u32 _i = 0; _i < sizeof(_plain) / sizeof(_plain[0]); _i++) {
        kana_toolbar_restyle_button(_plain[_i]);
    }

    // The rows: quiet buttons on a surface, the way on in the accent, Delete in red.
    kana_toolbar_menu* const _menus[] = { &_toolbar->selection_menu, &_toolbar->context_menu, &_toolbar->viewer_menu,
                                          &_toolbar->chart_menu, &_toolbar->browse_menu, &_toolbar->practice_menu, &_toolbar->check_menu,
                                          &_toolbar->album_menu, &_toolbar->album_page_menu, &_toolbar->practice_set_menu,
                                          &_toolbar->practice_summary_menu, &_toolbar->select_menu, &_toolbar->exam_setup_menu,
                                          &_toolbar->exam_preview_menu, &_toolbar->exam_menu, &_toolbar->exam_results_menu,
                                          &_toolbar->stats_menu, &_toolbar->scan_menu, &_toolbar->viewer_add_menu, &_toolbar->vocab_menu,
                                          &_toolbar->wordexam_setup_menu, &_toolbar->wordexam_menu, &_toolbar->wordexam_results_menu };
    for(u32 _m = 0; _m < sizeof(_menus) / sizeof(_menus[0]); _m++) {
        kana_toolbar_style_panel(_menus[_m]->panel, KANA_TOOLBAR_ROW_RADIUS, 1.0f);
        for(u32 _i = 0; _i < _menus[_m]->count; _i++) {
            if(_menus[_m]->buttons[_i] == NULL) {
                continue;   // left out (see kana_toolbar_menu_create)
            }
            kana_toolbar_restyle_quiet(_menus[_m]->buttons[_i]);
            kana_toolbar_button_round(_menus[_m]->buttons[_i], 12.0f);
        }
    }
    _toolbar->_scan_translate_shown = UINT8_MAX;   // its look again, over the restyle
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
        _toolbar->scan_menu.buttons[KANA_SCAN_MENU_WRITE],
        _toolbar->vocab_menu.buttons[KANA_VOCAB_MENU_REVIEW],
        _toolbar->wordexam_setup_menu.buttons[KANA_WORDEXAM_SETUP_START],
        _toolbar->wordexam_menu.buttons[KANA_WORDEXAM_MENU_NEXT],
    };
    for(u32 _i = 0; _i < sizeof(_primary) / sizeof(_primary[0]); _i++) {
        kana_toolbar_button_primary(_primary[_i]);
        kana_toolbar_button_round(_primary[_i], 12.0f);
        kana_toolbar_set_enabled(_primary[_i], rde_ui_button_as_node(_primary[_i])->interactable);
    }
    kana_toolbar_album_show_view(_toolbar);   // the order shown, or the exams
    _toolbar->_mark_shown_for = 0;   // the viewer's Study, restyled for its mark again
    _toolbar->_exam_shown     = UINT32_MAX;
    _toolbar->_vocab_shown    = UINT32_MAX;
    _toolbar->_wordexam_shown = UINT32_MAX;
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

RDE_INTERNAL void kana_toolbar_build(kana_toolbar* _toolbar, b8 _first);

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
    _toolbar->_scan_kept_shown      = UINT32_MAX;
    _toolbar->_scan_translate_shown = UINT8_MAX;
    _toolbar->_album_view_shown     = KANA_ALBUM_VIEW_CHARACTERS;
    _toolbar->show_hud = _show_hud;
    _toolbar->tool     = KANA_TOOL_DRAW;
    _toolbar->tool_before = KANA_TOOL_DRAW;
    _toolbar->vertical = true;

    // Every font is Slug with RDE's defaults: its glyph textures start small
    // and grow to what is drawn — more slots as more glyphs show at once,
    // wider ones as bigger glyphs come (a kanji like 鬱 takes ~4x a Latin
    // letter) — so nothing is measured per font, and no glyph is left out for
    // being too big (録 once was, with fixed budgets). Going to the background
    // gives the memory back (kana_toolbar_trim_fonts).
    const rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _toolbar->font = rde_font_load(KANA_TOOLBAR_FONT_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);
    // Japanese falls through to Noto Sans JP (its glyphs load as they are first used).
    _toolbar->font_jp = rde_font_load(KANA_TOOLBAR_FONT_JP_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);
    if(_toolbar->font != NULL && _toolbar->font_jp != NULL) {
        rde_font_add_fallback(_toolbar->font, _toolbar->font_jp);
    }
    // The icons (icons.h): Phosphor, last in line, and a font of its own for Fill.
    _toolbar->font_icons      = rde_font_load(KANA_TOOLBAR_FONT_ICONS_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);
    _toolbar->font_icons_fill = rde_font_load(KANA_TOOLBAR_FONT_ICONS_FILL_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);
    if(_toolbar->font != NULL && _toolbar->font_icons != NULL) {
        rde_font_add_fallback(_toolbar->font, _toolbar->font_icons);
    }
    kana_toolbar_icons_regular = _toolbar->font_icons != NULL ? _toolbar->font_icons : _toolbar->font;
    kana_toolbar_icons_fill    = _toolbar->font_icons_fill;
    kana_toolbar_glyph_font    = _toolbar->font;

    kana_toolbar_build(_toolbar, true);
}

// Every widget, in the language now (text.h): the canvas and all on it. Again
// (kana_toolbar_rebuild) when the language changes; _first: the bar's place too.
RDE_INTERNAL void kana_toolbar_build(kana_toolbar* _toolbar, b8 _first) {
    _toolbar->_text_revision = kana_text_revision();
    // CONSTANT_PIXEL: one UI unit = one window unit, so the bar is the same size
    // on every screen, and positions match the pen's.
    _toolbar->ui = rde_ui_canvas_create(_toolbar->window, RDE_UI_CANVAS_RENDER_MODE_SCREEN_OVERLAY, NULL);
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
    _toolbar->undo  = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_UNDO),       kana_toolbar_on_undo);
    _toolbar->redo  = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_REDO),       kana_toolbar_on_redo);
    _toolbar->draw  = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_DRAW),  kana_toolbar_on_draw);
    _toolbar->erase = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_ERASE), kana_toolbar_on_erase);
    _toolbar->lasso_tool = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_LASSO), kana_toolbar_on_lasso);
    _toolbar->clear = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_CLEAR), kana_toolbar_on_clear);
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

    _toolbar->color       = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_COLOR),  kana_toolbar_on_color);
    _toolbar->brush_scale = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_PAGE),   kana_toolbar_on_brush_scale);
    _toolbar->paper       = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_PAPER),  kana_toolbar_on_paper);
    _toolbar->rotate      = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_ROTATE), kana_toolbar_on_rotate);
    _toolbar->reset_view  = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_TOOL_RESET),  kana_toolbar_on_reset_view);
    _toolbar->camera      = kana_toolbar_button(_toolbar, _tools, kana_text(KANA_TEXT_SCAN_CAMERA), kana_toolbar_on_camera);
    {
        const struct { rde_ui_button* button; const c8* icon; } _icons[] = {
            { _toolbar->undo, KANA_ICON_UNDO }, { _toolbar->redo, KANA_ICON_REDO }, { _toolbar->draw, KANA_ICON_DRAW },
            { _toolbar->erase, KANA_ICON_ERASE }, { _toolbar->lasso_tool, KANA_ICON_LASSO }, { _toolbar->clear, KANA_ICON_TRASH },
            { _toolbar->brush_scale, KANA_ICON_PAGE }, { _toolbar->paper, KANA_ICON_PAPER_DOTS }, { _toolbar->rotate, KANA_ICON_ROTATE },
            { _toolbar->reset_view, KANA_ICON_RESET_VIEW }, { _toolbar->camera, KANA_ICON_CAMERA },
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
        const c8* const _names[KANA_PAPER_COUNT] = { kana_text(KANA_TEXT_PAPER_DOTS), kana_text(KANA_TEXT_PAPER_SQUARES), kana_text(KANA_TEXT_PAPER_LINES),
                                                     kana_text(KANA_TEXT_PAPER_NONE) };   // KANA_PAPER_ order
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
        const c8* const             _labels[KANA_SELECTION_COUNT]    = { kana_text(KANA_TEXT_SEL_CUT), kana_text(KANA_TEXT_SEL_COPY), kana_text(KANA_TEXT_SEL_COPY_TEXT),
                                                                         kana_translate_available() ? kana_text(KANA_TEXT_SCAN_TRANSLATE) : NULL, kana_text(KANA_TEXT_SEL_SAVE_WORD),
                                                                         kana_text(KANA_TEXT_SEL_DUPLICATE), kana_text(KANA_TEXT_SEL_CHECK), kana_text(KANA_TEXT_DELETE) };
        const c8* const             _icons[KANA_SELECTION_COUNT]     = { KANA_ICON_CUT, KANA_ICON_COPY, KANA_ICON_TEXT_COPY, KANA_ICON_TRANSLATE, KANA_ICON_BOOKMARK, KANA_ICON_DUPLICATE,
                                                                         KANA_ICON_SEARCH, KANA_ICON_TRASH };
        const rde_ui_event_callback _callbacks[KANA_SELECTION_COUNT] = { kana_toolbar_on_cut, kana_toolbar_on_copy, kana_toolbar_on_copy_text, kana_toolbar_on_translate,
                                                                         kana_toolbar_on_save_word, kana_toolbar_on_duplicate, kana_toolbar_on_check,
                                                                         kana_toolbar_on_delete_selection };
        kana_toolbar_menu_create(_toolbar, &_toolbar->selection_menu, _root, _labels, _icons, _callbacks, KANA_SELECTION_COUNT);
    }
    {
        const c8* const             _labels[KANA_CONTEXT_COUNT]    = { kana_text(KANA_TEXT_CTX_PASTE), kana_text(KANA_TEXT_CTX_PASTE_TEXT), kana_text(KANA_TEXT_CTX_SCAN),
                                                                       kana_text(KANA_TEXT_CTX_SELECT_ALL) };
        const c8* const             _icons[KANA_CONTEXT_COUNT]     = { KANA_ICON_PASTE, KANA_ICON_TEXT_PASTE, KANA_ICON_SCAN, KANA_ICON_SELECT_ALL };
        const rde_ui_event_callback _callbacks[KANA_CONTEXT_COUNT] = { kana_toolbar_on_paste, kana_toolbar_on_paste_text, kana_toolbar_on_scan, kana_toolbar_on_select_all };
        kana_toolbar_menu_create(_toolbar, &_toolbar->context_menu, _root, _labels, _icons, _callbacks, KANA_CONTEXT_COUNT);
    }
    {
        const c8* const             _labels[KANA_VIEWER_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_PREV), kana_text(KANA_TEXT_REPLAY), kana_text(KANA_TEXT_NEXT), kana_text(KANA_TEXT_STUDY),
                                                                       kana_text(KANA_TEXT_SHEET), kana_text(KANA_TEXT_PRACTICE) };
        const c8* const             _icons[KANA_VIEWER_COUNT]     = { KANA_ICON_BACK, KANA_ICON_PREV, KANA_ICON_REPLAY, KANA_ICON_NEXT, KANA_ICON_STAR, KANA_ICON_PRINT, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_VIEWER_COUNT] = { kana_toolbar_on_viewer_back, kana_toolbar_on_viewer_prev, kana_toolbar_on_viewer_replay,
                                                                      kana_toolbar_on_viewer_next, kana_toolbar_on_viewer_study, kana_toolbar_on_viewer_sheet,
                                                                      kana_toolbar_on_viewer_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->viewer_menu, _root, _labels, _icons, _callbacks, KANA_VIEWER_COUNT);
    }
    {
        const c8* const             _labels[KANA_CHECK_MENU_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_STROKE_ORDER), kana_text(KANA_TEXT_PRACTICE) };
        const c8* const             _icons[KANA_CHECK_MENU_COUNT]     = { KANA_ICON_BACK, KANA_ICON_STROKE_ORDER, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_CHECK_MENU_COUNT] = { kana_toolbar_on_check_back, kana_toolbar_on_check_order, kana_toolbar_on_check_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->check_menu, _root, _labels, _icons, _callbacks, KANA_CHECK_MENU_COUNT);

        _toolbar->check_field = rde_ui_text_editor_create(_toolbar->font, NULL);
        rde_ui_text_editor_set_multiline(_toolbar->check_field, false);
        rde_ui_text_editor_set_font_size(_toolbar->check_field, KANA_TOOLBAR_FIELD_PX);
        rde_ui_text_editor_set_max_chars(_toolbar->check_field, KANA_CHECK_MEANT);
        rde_ui_text_editor_set_placeholder(_toolbar->check_field, kana_text(KANA_TEXT_CHECK_FIELD));
        rde_ui_text_editor_set_content_insets(_toolbar->check_field, 12.0f, 8.0f, 12.0f, 8.0f);
        rde_ui_text_editor_add_plugin(_toolbar->check_field, rde_ui_text_editor_plugin_ime_get());   // the keyboard's composition, at the caret
        rde_ui_node* _field = rde_ui_text_editor_as_node(_toolbar->check_field);
        rde_ui_node_set_user_data(_field, _toolbar);
        rde_ui_text_editor_set_on_submit(_toolbar->check_field, kana_toolbar_on_check_meant);
        kana_toolbar_field_box(_root, _toolbar->check_field);
        rde_ui_node_set_active(kana_toolbar_field_node(_toolbar->check_field), false);
    }
    {
        const c8* const             _labels[KANA_CHART_MENU_COUNT]    = { kana_text(KANA_TEXT_HIRAGANA), kana_text(KANA_TEXT_KATAKANA), kana_text(KANA_TEXT_SELECT),
                                                                          kana_text(KANA_TEXT_PRACTICE), kana_text(KANA_TEXT_CLOSE) };
        const c8* const             _icons[KANA_CHART_MENU_COUNT]     = { "\xE3\x81\x82", "\xE3\x82\xA2", KANA_ICON_SELECT, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_CHART_MENU_COUNT] = { kana_toolbar_on_chart_hiragana, kana_toolbar_on_chart_katakana, kana_toolbar_on_select_mode,
                                                                          kana_toolbar_on_chart_practice, kana_toolbar_on_chart_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->chart_menu, _root, _labels, _icons, _callbacks, KANA_CHART_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_BROWSE_MENU_COUNT]    = { kana_text(KANA_TEXT_SELECT), kana_text(KANA_TEXT_PRACTICE), kana_text(KANA_TEXT_CLOSE) };
        const c8* const             _icons[KANA_BROWSE_MENU_COUNT]     = { KANA_ICON_SELECT, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_BROWSE_MENU_COUNT] = { kana_toolbar_on_select_mode, kana_toolbar_on_browse_practice, kana_toolbar_on_browse_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->browse_menu, _root, _labels, _icons, _callbacks, KANA_BROWSE_MENU_COUNT);
    }
    {
        c8 _practice0[48];
        KANA_TEXTF(_practice0, KANA_TEXT_PRACTICE_N, KANA_TN(0));
        const c8* const             _labels[KANA_SELECT_MENU_COUNT]    = { kana_text(KANA_TEXT_ALL), kana_text(KANA_TEXT_NONE), kana_text(KANA_TEXT_STUDY),
                                                                           kana_text(KANA_TEXT_EXAM), kana_text(KANA_TEXT_SHEET), kana_text(KANA_TEXT_SAVE_LIST), _practice0,
                                                                           kana_text(KANA_TEXT_DONE) };
        const c8* const             _icons[KANA_SELECT_MENU_COUNT]     = { KANA_ICON_SELECT_ALL, KANA_ICON_SELECT_NONE, KANA_ICON_STAR, KANA_ICON_EXAM, KANA_ICON_PRINT, KANA_ICON_LISTS,
                                                                           KANA_ICON_PEN, KANA_ICON_CHECK };
        const rde_ui_event_callback _callbacks[KANA_SELECT_MENU_COUNT] = { kana_toolbar_on_ticks_all, kana_toolbar_on_ticks_none, kana_toolbar_on_ticks_study,
                                                                           kana_toolbar_on_ticks_exam, kana_toolbar_on_ticks_sheet, kana_toolbar_on_ticks_list,
                                                                           kana_toolbar_on_ticks_practice, kana_toolbar_on_ticks_done };
        kana_toolbar_menu_create(_toolbar, &_toolbar->select_menu, _root, _labels, _icons, _callbacks, KANA_SELECT_MENU_COUNT);
    }
    {
        c8 _next0[48];
        KANA_TEXTF(_next0, KANA_TEXT_NEXT_N, KANA_TN(0));
        const c8* const             _labels[KANA_EXAM_SETUP_COUNT]    = { kana_text(KANA_TEXT_CLOSE), _next0 };
        const c8* const             _icons[KANA_EXAM_SETUP_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_EXAM_SETUP_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_next_stage };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_setup_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_SETUP_COUNT);
    }
    {
        c8 _start0[48];
        KANA_TEXTF(_start0, KANA_TEXT_START_N, KANA_TN(0));
        const c8* const             _labels[KANA_EXAM_PREVIEW_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_ALL), kana_text(KANA_TEXT_NONE), _start0 };
        const c8* const             _icons[KANA_EXAM_PREVIEW_COUNT]     = { KANA_ICON_BACK, KANA_ICON_SELECT_ALL, KANA_ICON_SELECT_NONE, KANA_ICON_PLAY };
        const rde_ui_event_callback _callbacks[KANA_EXAM_PREVIEW_COUNT] = { kana_toolbar_on_exam_back, kana_toolbar_on_exam_all, kana_toolbar_on_exam_none,
                                                                            kana_toolbar_on_exam_start };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_preview_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_PREVIEW_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_MENU_COUNT]    = { kana_text(KANA_TEXT_QUIT), kana_text(KANA_TEXT_UNDO), kana_text(KANA_TEXT_CLEAR), kana_text(KANA_TEXT_NEXT) };
        const c8* const             _icons[KANA_EXAM_MENU_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_EXAM_MENU_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_undo, kana_toolbar_on_exam_clear,
                                                                         kana_toolbar_on_exam_next };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_EXAM_RESULTS_COUNT]    = { kana_text(KANA_TEXT_DONE), kana_text(KANA_TEXT_RETRY_WRONG), kana_text(KANA_TEXT_PRACTICE_WRONG) };
        const c8* const             _icons[KANA_EXAM_RESULTS_COUNT]     = { KANA_ICON_CHECK, KANA_ICON_RETRY, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_EXAM_RESULTS_COUNT] = { kana_toolbar_on_exam_close, kana_toolbar_on_exam_retry, kana_toolbar_on_exam_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->exam_results_menu, _root, _labels, _icons, _callbacks, KANA_EXAM_RESULTS_COUNT);
    }
    {
        const c8* const             _labels[KANA_STATS_MENU_COUNT]    = { kana_text(KANA_TEXT_CLOSE) };
        const c8* const             _icons[KANA_STATS_MENU_COUNT]     = { KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_STATS_MENU_COUNT] = { kana_toolbar_on_stats_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->stats_menu, _root, _labels, _icons, _callbacks, KANA_STATS_MENU_COUNT);
    }
    {
        c8 _review0[48];
        KANA_TEXTF(_review0, KANA_TEXT_VOCAB_REVIEW_N, KANA_TN(0));
        const c8* const             _labels[KANA_VOCAB_MENU_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_VOCAB_ADD_WORD), _review0, kana_text(KANA_TEXT_EXAM),
                                                                          kana_text(KANA_TEXT_SHEET), kana_text(KANA_TEXT_PRACTICE) };
        const c8* const             _icons[KANA_VOCAB_MENU_COUNT]     = { KANA_ICON_BACK, KANA_ICON_PLUS, KANA_ICON_RETRY, KANA_ICON_EXAM, KANA_ICON_PRINT, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_VOCAB_MENU_COUNT] = { kana_toolbar_on_vocab_back, kana_toolbar_on_vocab_word, kana_toolbar_on_vocab_review,
                                                                          kana_toolbar_on_vocab_exam, kana_toolbar_on_vocab_sheet, kana_toolbar_on_vocab_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->vocab_menu, _root, _labels, _icons, _callbacks, KANA_VOCAB_MENU_COUNT);
        _toolbar->_vocab_shown = UINT32_MAX;
    }
    {
        c8 _start0[48];
        KANA_TEXTF(_start0, KANA_TEXT_START_N, KANA_TN(0));
        const c8* const             _labels[KANA_WORDEXAM_SETUP_COUNT]    = { kana_text(KANA_TEXT_CLOSE), _start0 };
        const c8* const             _icons[KANA_WORDEXAM_SETUP_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_WORDEXAM_SETUP_COUNT] = { kana_toolbar_on_wordexam_close, kana_toolbar_on_wordexam_start };
        kana_toolbar_menu_create(_toolbar, &_toolbar->wordexam_setup_menu, _root, _labels, _icons, _callbacks, KANA_WORDEXAM_SETUP_COUNT);
    }
    {
        const c8* const             _labels[KANA_WORDEXAM_MENU_COUNT]    = { kana_text(KANA_TEXT_QUIT), kana_text(KANA_TEXT_UNDO), kana_text(KANA_TEXT_CLEAR), kana_text(KANA_TEXT_NEXT) };
        const c8* const             _icons[KANA_WORDEXAM_MENU_COUNT]     = { KANA_ICON_CLOSE, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_WORDEXAM_MENU_COUNT] = { kana_toolbar_on_wordexam_close, kana_toolbar_on_wordexam_undo, kana_toolbar_on_wordexam_clear,
                                                                             kana_toolbar_on_wordexam_next };
        kana_toolbar_menu_create(_toolbar, &_toolbar->wordexam_menu, _root, _labels, _icons, _callbacks, KANA_WORDEXAM_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_WORDEXAM_RESULTS_COUNT]    = { kana_text(KANA_TEXT_DONE), kana_text(KANA_TEXT_RETRY_WRONG) };
        const c8* const             _icons[KANA_WORDEXAM_RESULTS_COUNT]     = { KANA_ICON_CHECK, KANA_ICON_RETRY };
        const rde_ui_event_callback _callbacks[KANA_WORDEXAM_RESULTS_COUNT] = { kana_toolbar_on_wordexam_close, kana_toolbar_on_wordexam_retry };
        kana_toolbar_menu_create(_toolbar, &_toolbar->wordexam_results_menu, _root, _labels, _icons, _callbacks, KANA_WORDEXAM_RESULTS_COUNT);
        _toolbar->_wordexam_shown = UINT32_MAX;
    }
    {
        const c8* const             _labels[KANA_SCAN_MENU_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_SCAN_CAMERA), kana_text(KANA_TEXT_SCAN_PHOTOS),
                                                                         kana_translate_available() ? kana_text(KANA_TEXT_SCAN_TRANSLATE) : NULL,
                                                                         kana_text(KANA_TEXT_SCAN_WRITE_N) };
        const c8* const             _icons[KANA_SCAN_MENU_COUNT]     = { KANA_ICON_BACK, KANA_ICON_CAMERA, KANA_ICON_IMAGE, KANA_ICON_TRANSLATE, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_SCAN_MENU_COUNT] = { kana_toolbar_on_scan_back, kana_toolbar_on_scan_camera, kana_toolbar_on_scan_photos,
                                                                         kana_toolbar_on_scan_translate, kana_toolbar_on_scan_write };
        kana_toolbar_menu_create(_toolbar, &_toolbar->scan_menu, _root, _labels, _icons, _callbacks, KANA_SCAN_MENU_COUNT);
    }
    {
        const c8* const             _labels[KANA_VIEWER_ADD_COUNT]    = { kana_text(KANA_TEXT_DONE), kana_text(KANA_TEXT_TYPE_YOUR_OWN) };
        const c8* const             _icons[KANA_VIEWER_ADD_COUNT]     = { KANA_ICON_CHECK, KANA_ICON_KEYBOARD };
        const rde_ui_event_callback _callbacks[KANA_VIEWER_ADD_COUNT] = { kana_toolbar_on_viewer_add_done, kana_toolbar_on_viewer_add_type };
        kana_toolbar_menu_create(_toolbar, &_toolbar->viewer_add_menu, _root, _labels, _icons, _callbacks, KANA_VIEWER_ADD_COUNT);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_UNDO), kana_text(KANA_TEXT_CLEAR), kana_text(KANA_TEXT_SCORE),
                                                                        kana_text(KANA_TEXT_FEWER), kana_text(KANA_TEXT_MORE), kana_text(KANA_TEXT_GUIDED) };
        const c8* const             _icons[KANA_PRACTICE_COUNT]     = { KANA_ICON_BACK, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_TARGET, KANA_ICON_MINUS, KANA_ICON_PLUS, KANA_ICON_GUIDED };
        const rde_ui_event_callback _callbacks[KANA_PRACTICE_COUNT] = { kana_toolbar_on_practice_back, kana_toolbar_on_practice_undo, kana_toolbar_on_practice_clear,
                                                                        kana_toolbar_on_practice_score, kana_toolbar_on_practice_fewer, kana_toolbar_on_practice_more,
                                                                        kana_toolbar_on_practice_guided };
        kana_toolbar_menu_create(_toolbar, &_toolbar->practice_menu, _root, _labels, _icons, _callbacks, KANA_PRACTICE_COUNT);
    }
    {
        const c8* const             _labels[KANA_ALBUM_MENU_COUNT]    = { kana_text(KANA_TEXT_ALBUM_WEAKEST), kana_text(KANA_TEXT_ALBUM_RECENT), kana_text(KANA_TEXT_ALBUM_MOST),
                                                                          kana_text(KANA_TEXT_EXAMS), kana_text(KANA_TEXT_PRACTICE), kana_text(KANA_TEXT_CLOSE) };
        const c8* const             _icons[KANA_ALBUM_MENU_COUNT]     = { KANA_ICON_WEAKEST, KANA_ICON_CLOCK, KANA_ICON_STACK, KANA_ICON_EXAM, KANA_ICON_PEN, KANA_ICON_CLOSE };
        const rde_ui_event_callback _callbacks[KANA_ALBUM_MENU_COUNT] = { kana_toolbar_on_album_sort, kana_toolbar_on_album_sort, kana_toolbar_on_album_sort,
                                                                          kana_toolbar_on_album_exams, kana_toolbar_on_album_practice_weakest, kana_toolbar_on_album_close };
        kana_toolbar_menu_create(_toolbar, &_toolbar->album_menu, _root, _labels, _icons, _callbacks, KANA_ALBUM_MENU_COUNT);
        for(u32 _i = 0; _i < KANA_ALBUM_SORT_COUNT; _i++) {
            _toolbar->album_sort_refs[_i] = (kana_toolbar_chip_ref){ _toolbar, _i };
            rde_ui_button_set_on_click(_toolbar->album_menu.buttons[_i], kana_toolbar_on_album_sort, &_toolbar->album_sort_refs[_i]);
        }
    }
    {
        const c8* const             _labels[KANA_ALBUM_PAGE_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_PRACTICE) };
        const c8* const             _icons[KANA_ALBUM_PAGE_COUNT]     = { KANA_ICON_BACK, KANA_ICON_PEN };
        const rde_ui_event_callback _callbacks[KANA_ALBUM_PAGE_COUNT] = { kana_toolbar_on_album_back, kana_toolbar_on_album_practice };
        kana_toolbar_menu_create(_toolbar, &_toolbar->album_page_menu, _root, _labels, _icons, _callbacks, KANA_ALBUM_PAGE_COUNT);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_SET_COUNT]    = { kana_text(KANA_TEXT_BACK), kana_text(KANA_TEXT_UNDO), kana_text(KANA_TEXT_CLEAR), kana_text(KANA_TEXT_SCORE),
                                                                            kana_text(KANA_TEXT_GUIDED), kana_text(KANA_TEXT_NEXT) };
        const c8* const             _icons[KANA_PRACTICE_SET_COUNT]     = { KANA_ICON_BACK, KANA_ICON_UNDO, KANA_ICON_TRASH, KANA_ICON_TARGET, KANA_ICON_GUIDED, KANA_ICON_ARROW_RIGHT };
        const rde_ui_event_callback _callbacks[KANA_PRACTICE_SET_COUNT] = { kana_toolbar_on_practice_done, kana_toolbar_on_practice_undo, kana_toolbar_on_practice_clear,
                                                                            kana_toolbar_on_practice_score, kana_toolbar_on_practice_guided, kana_toolbar_on_practice_next };
        kana_toolbar_menu_create(_toolbar, &_toolbar->practice_set_menu, _root, _labels, _icons, _callbacks, KANA_PRACTICE_SET_COUNT);
    }
    {
        const c8* const             _labels[KANA_PRACTICE_SUMMARY_COUNT]    = { kana_text(KANA_TEXT_WEAKEST_AGAIN), kana_text(KANA_TEXT_DONE) };
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
            _toolbar->filter_chips[_i] = kana_toolbar_button(_toolbar, _bar, kana_toolbar_filter_label(_i), kana_toolbar_on_filter);
            rde_ui_button_set_on_click(_toolbar->filter_chips[_i], kana_toolbar_on_filter, &_toolbar->filter_refs[_i]);
        }
        for(u32 _i = 0; _i < KANA_SORT_COUNT; _i++) {
            _toolbar->sort_refs[_i]  = (kana_toolbar_chip_ref){ _toolbar, _i };
            _toolbar->sort_chips[_i] = kana_toolbar_button(_toolbar, _bar, kana_text(KANA_SORT_TEXTS[_i]), kana_toolbar_on_sort);
            rde_ui_button_set_on_click(_toolbar->sort_chips[_i], kana_toolbar_on_sort, &_toolbar->sort_refs[_i]);
        }
        _toolbar->draw_toggle  = kana_toolbar_button(_toolbar, _bar, kana_text(KANA_TEXT_BROWSE_DRAW),  kana_toolbar_on_draw_toggle);
        _toolbar->parts_toggle = kana_toolbar_button(_toolbar, _bar, kana_text(KANA_TEXT_BROWSE_PARTS), kana_toolbar_on_parts_toggle);
        if(!kana_browse_parts_available(_toolbar->browse)) {
            kana_toolbar_set_enabled(_toolbar->parts_toggle, false);   // character data baked before parts
        }
        _toolbar->pad_clear    = kana_toolbar_button(_toolbar, _bar, kana_text(KANA_TEXT_CLEAR), kana_toolbar_on_pad_clear);
        kana_toolbar_icon(_toolbar->draw_toggle, KANA_ICON_SCRIBBLE, KANA_TOOLBAR_ICON_LEFT, 15.0f);
        kana_toolbar_icon(_toolbar->parts_toggle, "\xE9\x83\xA8", KANA_TOOLBAR_ICON_LEFT, 13.0f);   // 部
        kana_toolbar_icon(_toolbar->pad_clear, KANA_ICON_CLOSE, KANA_TOOLBAR_ICON_LEFT, 14.0f);

        _toolbar->search_field = rde_ui_text_editor_create(_toolbar->font, NULL);
        rde_ui_text_editor_set_multiline(_toolbar->search_field, false);
        rde_ui_text_editor_set_font_size(_toolbar->search_field, KANA_TOOLBAR_FIELD_PX);
        c8 _hint[160];
        snprintf(_hint, sizeof(_hint), KANA_ICON_SEARCH "  %s", kana_text(KANA_TEXT_SEARCH_HINT));
        rde_ui_text_editor_set_placeholder(_toolbar->search_field, _hint);
        rde_ui_text_editor_set_content_insets(_toolbar->search_field, 12.0f, 8.0f, 12.0f, 8.0f);
        rde_ui_node* _field = rde_ui_text_editor_as_node(_toolbar->search_field);
        rde_ui_node_set_user_data(_field, _toolbar);
        rde_ui_text_editor_set_on_change(_toolbar->search_field, kana_toolbar_on_search_changed);
        kana_toolbar_field_box(_bar, _toolbar->search_field);

        rde_ui_node_set_active(_bar, false);
    }

    // The side panel and Settings last: over everything else.
    kana_side_create(_toolbar, _root);
    kana_wordcard_create(_toolbar, _root);   // over everything: it opens from any screen

    // Start on the right edge, vertically centred.
    if(_first) {
        const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
        _toolbar->center = (rde_vec_2F){ _screen.x - 60.0f, _screen.y * 0.5f };
    }

    kana_toolbar_layout(_toolbar);
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_set_paper_open(_toolbar, false);
    kana_toolbar_apply_theme(_toolbar);
    kana_toolbar_update(_toolbar);
}

// The language changed: every widget built again in it, the bar where it was,
// the tool, and Settings still open if it was (it is where the language is chosen).
RDE_INTERNAL void kana_toolbar_rebuild(kana_toolbar* _toolbar) {
    kana_toolbar _kept = *_toolbar;
    kana_side_forget(_toolbar);
    rde_ui_canvas_destroy(_toolbar->ui);

    memset(_toolbar, 0, sizeof(*_toolbar));
    _toolbar->window          = _kept.window;
    _toolbar->ink             = _kept.ink;
    _toolbar->view            = _kept.view;
    _toolbar->lasso           = _kept.lasso;
    _toolbar->viewer          = _kept.viewer;
    _toolbar->browse          = _kept.browse;
    _toolbar->chart           = _kept.chart;
    _toolbar->practice        = _kept.practice;
    _toolbar->album           = _kept.album;
    _toolbar->notes           = _kept.notes;
    _toolbar->check           = _kept.check;
    _toolbar->selection       = _kept.selection;
    _toolbar->exam            = _kept.exam;
    _toolbar->stats           = _kept.stats;
    _toolbar->scan            = _kept.scan;
    _toolbar->show_hud        = _kept.show_hud;
    _toolbar->font            = _kept.font;
    _toolbar->font_jp         = _kept.font_jp;
    _toolbar->font_icons      = _kept.font_icons;
    _toolbar->font_icons_fill = _kept.font_icons_fill;
    _toolbar->tool            = _kept.tool;
    _toolbar->vertical        = _kept.vertical;
    _toolbar->center          = _kept.center;
    _toolbar->minimized       = _kept.minimized;
    // What these own moves over (the old struct is gone).
    _toolbar->text_reader        = _kept.text_reader;
    _toolbar->_text_reader_ready = _kept._text_reader_ready;
    _toolbar->text_for_translate   = _kept.text_for_translate;
    _toolbar->text_for_vocab       = _kept.text_for_vocab;
    _toolbar->translation_state    = _kept.translation_state;
    _toolbar->translation_word_count = _kept.translation_word_count;
    memcpy(_toolbar->translation_words, _kept.translation_words, sizeof(_toolbar->translation_words));
    _toolbar->translation_ticket   = _kept.translation_ticket;
    _toolbar->translation_of       = _kept.translation_of;
    _toolbar->translation_prepared = _kept.translation_prepared;
    memcpy(_toolbar->translation_from, _kept.translation_from, sizeof(_toolbar->translation_from));
    memcpy(_toolbar->translation, _kept.translation, sizeof(_toolbar->translation));
    _toolbar->_text_clip         = _kept._text_clip;
    _toolbar->notice_at          = _kept.notice_at;
    memcpy(_toolbar->notice, _kept.notice, sizeof(_toolbar->notice));
    _toolbar->_album_practice_shown = UINT32_MAX;
    _toolbar->_album_view_shown     = UINT32_MAX;
    _toolbar->_scan_kept_shown      = UINT32_MAX;
    _toolbar->_scan_translate_shown = UINT8_MAX;
    kana_toolbar_build(_toolbar, false);
    _toolbar->side.open          = _kept.side.open;
    _toolbar->side.settings_open = _kept.side.settings_open;
}

void kana_toolbar_follow_language(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL && _toolbar->_text_revision != kana_text_revision()) {
        kana_toolbar_rebuild(_toolbar);
    }
}

void kana_toolbar_trim_fonts(kana_toolbar* _toolbar) {
    rde_font* const _fonts[] = { _toolbar->font, _toolbar->font_jp, _toolbar->font_icons, _toolbar->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(_fonts[_i] != NULL) {
            rde_font_trim(_fonts[_i]);
        }
    }
}

void kana_toolbar_destroy(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL) {
        rde_ui_canvas_destroy(_toolbar->ui);
        _toolbar->ui = NULL;
    }
    if(_toolbar->_text_reader_ready) {
        kana_textink_reader_destroy(&_toolbar->text_reader);
        _toolbar->_text_reader_ready = false;
    }
    rde_arr* const _clip[] = { &_toolbar->_text_clip.strokes, &_toolbar->_text_clip.points };
    for(u32 _i = 0; _i < 2u; _i++) {
        if(rde_arr_is_inited(_clip[_i])) {
            rde_arr_free(_clip[_i]);
        }
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
    if(kana_side_hit(_toolbar, _p) || _toolbar->word.open) {
        return true;
    }

    // Translate with Google's card (kept in Kana's screen space).
    if(_toolbar->translation_state != KANA_TOOLBAR_TRANSLATION_NONE && _screen.x >= _toolbar->translation_min.x && _screen.x <= _toolbar->translation_max.x &&
       _screen.y >= _toolbar->translation_min.y && _screen.y <= _toolbar->translation_max.y) {
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
                                          &_toolbar->stats_menu, &_toolbar->scan_menu, &_toolbar->viewer_add_menu, &_toolbar->vocab_menu,
                                          &_toolbar->wordexam_setup_menu, &_toolbar->wordexam_menu, &_toolbar->wordexam_results_menu };
    for(u32 _i = 0; _i < sizeof(_menus) / sizeof(_menus[0]); _i++) {
        if(_menus[_i]->open && kana_toolbar_rect_contains(_menus[_i]->center, _menus[_i]->size, _p)) {
            return true;
        }
    }

    return false;
}
