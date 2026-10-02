#include "drawing/widgets/toolbar.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/icons.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See toolbar.h.
// ===========================================================================

// Geometry, UI canvas units (= window units: the canvas is CONSTANT_PIXEL).
#define FUDE_TOOLBAR_BUTTON_W    44.0f
#define FUDE_TOOLBAR_BUTTON_H    44.0f
#define FUDE_TOOLBAR_TOOL_ICON_PX 17.0f   // ~22 on screen
#define FUDE_TOOLBAR_DOT         22.0f    // Color's dot
#define FUDE_TOOLBAR_GRIP        22.0f
#define FUDE_TOOLBAR_GRIP_PX     15.0f    // its dots' size
#define FUDE_TOOLBAR_SLIDER_LEN  110.0f
#define FUDE_TOOLBAR_SLIDER_W    20.0f
#define FUDE_TOOLBAR_SEPARATOR   9.0f     // along the bar: a hairline and room either side
#define FUDE_TOOLBAR_SPACING     4.0f
#define FUDE_TOOLBAR_PADDING     6.0f
#define FUDE_TOOLBAR_RADIUS      16.0f
#define FUDE_TOOLBAR_MINIMIZED   48.0f    // the folded bar's length: just its grip, big enough for a finger
#define FUDE_TOOLBAR_DOUBLE_TAP  0.35     // seconds between a double tap's two taps
#define FUDE_TOOLBAR_DOUBLE_TAP_SLOP 32.0f   // and how far apart they can be
#define FUDE_TOOLBAR_SWATCH      36.0f
#define FUDE_TOOLBAR_CHOICE_W    72.0f    // the paper panel's choices: an icon over its name
#define FUDE_TOOLBAR_CHOICE_H    60.0f
#define FUDE_TOOLBAR_SWATCH_COLS 4u
#define FUDE_TOOLBAR_PALETTE_GAP 8.0f

// The palette: every colour but the first is fixed; the first is the theme's ink.
RDE_INTERNAL const rde_color FUDE_TOOLBAR_PALETTE[FUDE_TOOLBAR_PALETTE_COUNT] = {
    {   0,   0,   0,   0 },   // FUDE_THEME_INK: the theme's ink, dark on light pages, light on dark ones
    { 128, 128, 136, 255 },   // pencil grey: readable on every page
    { 230,  72,  72, 255 },
    { 240, 150,  50, 255 },
    { 240, 210,  70, 255 },
    {  90, 190, 110, 255 },
    {  80, 140, 235, 255 },
    { 170, 110, 220, 255 },
};

// The marker's palette (Mark chosen): see-through, its alpha FUDE_INK_MARKER_ALPHA.
RDE_INTERNAL const rde_color FUDE_TOOLBAR_MARKERS[FUDE_TOOLBAR_PALETTE_COUNT] = {
    { 255, 214,   0, FUDE_INK_MARKER_ALPHA },   // the classic yellow
    { 120, 220,  60, FUDE_INK_MARKER_ALPHA },
    { 255, 120, 180, FUDE_INK_MARKER_ALPHA },
    {  70, 190, 255, FUDE_INK_MARKER_ALPHA },
    { 255, 150,  40, FUDE_INK_MARKER_ALPHA },
    { 170, 120, 255, FUDE_INK_MARKER_ALPHA },
    { 255,  70,  70, FUDE_INK_MARKER_ALPHA },
    { 150, 150, 160, FUDE_INK_MARKER_ALPHA },
};

// A colour as a swatch or the dot shows it: whole (a marker's alpha would only wash it out).
RDE_INTERNAL rde_color fude_toolbar_solid(rde_color _c) {
    if(_c.r == 0 && _c.g == 0 && _c.b == 0 && _c.a == 0) {
        return fude_theme_resolve(_c);   // the theme's ink
    }
    _c.a = 255;
    return _c;
}

RDE_INTERNAL b8 fude_toolbar_contains(rde_vec_2F _center, rde_vec_2F _size, rde_vec_2F _p) {
    return _p.x >= _center.x - _size.x * 0.5f && _p.x <= _center.x + _size.x * 0.5f &&
           _p.y >= _center.y - _size.y * 0.5f && _p.y <= _center.y + _size.y * 0.5f;
}

// A button's centre as laid out last frame (UI units).
RDE_INTERNAL rde_vec_2F fude_toolbar_center_of(rde_ui_button* _button) {
    const rde_ui_node* _n = rde_ui_button_as_node(_button);
    return (rde_vec_2F){ _n->rect.computed_position.x + _n->rect.computed_size.x * 0.5f, _n->rect.computed_position.y + _n->rect.computed_size.y * 0.5f };
}

// --- state → widgets -----------------------------------------------------------

void fude_toolbar_refresh(fude_toolbar* _toolbar) {
    fude_app* _app = _toolbar->app;
    const struct { rde_ui_button* button; FUDE_TOOL_ tool; } _tools[] = {
        { _toolbar->draw,       FUDE_TOOL_DRAW  },
        { _toolbar->mark,       FUDE_TOOL_MARK  },
        { _toolbar->erase,      FUDE_TOOL_ERASE },
        { _toolbar->lasso_tool, FUDE_TOOL_LASSO },
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        if(_tools[_i].tool == _toolbar->tool) { fude_kit_button_selected(_tools[_i].button); }
        else                                  { fude_kit_button_quiet(_tools[_i].button); }
    }
    // The hand: chosen while a finger writes.
    if(_app->finger_writes) { fude_kit_button_selected(_toolbar->finger); }
    else                    { fude_kit_button_quiet(_toolbar->finger); }

    // Color's dot IS the current colour (the theme's ink, when that is it; the
    // marker's, marking), ringed so white or the page's own colour still shows.
    // The palette the same: the marker's colours while it is in hand.
    const b8 _marking = _toolbar->tool == FUDE_TOOL_MARK;
    _app->ink->marking = _marking;
    rde_ui_style _dot = fude_kit_style(fude_toolbar_solid(_marking ? _app->ink->marker_color : _app->ink->color), FUDE_TOOLBAR_DOT * 0.5f);
    _dot.border_width = 2.0f;
    _dot.border_color = fude_theme_active()->outline;
    rde_ui_image_set_style(_toolbar->color_dot, RDE_UI_STATE_NORMAL, _dot);
    if(_toolbar->palette_open) { fude_kit_button_selected(_toolbar->color); }
    else                       { fude_kit_button_quiet(_toolbar->color); }

    // Page or Screen: what the brush's width keeps to.
    const b8 _page = _app->ink->brush_scale == FUDE_INK_BRUSH_SCALE_PAGE;
    rde_ui_button_set_text(_toolbar->brush_scale, fude_text(_page ? FUDE_TEXT_TOOL_PAGE : FUDE_TEXT_TOOL_SCREEN));
    fude_kit_icon(_toolbar->brush_scale, _page ? FUDE_ICON_PAGE : FUDE_ICON_TABLET, FUDE_KIT_ICON_ONLY, FUDE_TOOLBAR_TOOL_ICON_PX);

    // The paper panel shows the page's own paper chosen: it follows the canvas
    // open. Paper shows it too.
    static const c8* const _paper_icons[FUDE_PAPER_COUNT] = { FUDE_ICON_PAPER_DOTS, FUDE_ICON_PAPER_SQUARES, FUDE_ICON_PAPER_LINES, FUDE_ICON_PAPER_NONE };
    const FUDE_PAPER_ _now = _app->canvas->page.paper;
    for(u32 _i = 0; _i < FUDE_PAPER_COUNT; _i++) {
        if((FUDE_PAPER_)_i == _now) { fude_kit_button_selected(_toolbar->paper_choices[_i]); }
        else                        { fude_kit_button_quiet(_toolbar->paper_choices[_i]); }
    }
    fude_kit_icon(_toolbar->paper, _paper_icons[(u32)_now < FUDE_PAPER_COUNT ? (u32)_now : 0u], FUDE_KIT_ICON_ONLY, FUDE_TOOLBAR_TOOL_ICON_PX);
    if(_toolbar->paper_open) { fude_kit_button_selected(_toolbar->paper); }
    else                     { fude_kit_button_quiet(_toolbar->paper); }
    _toolbar->_paper_shown = _now;

    for(u32 _i = 0; _i < FUDE_TOOLBAR_PALETTE_COUNT; _i++) {
        fude_kit_button_colors(_toolbar->swatches[_i], fude_toolbar_solid(_marking ? FUDE_TOOLBAR_MARKERS[_i] : FUDE_TOOLBAR_PALETTE[_i]), 2.0f, fude_theme_active()->outline);
        fude_kit_button_round(_toolbar->swatches[_i], FUDE_TOOLBAR_SWATCH * 0.5f);
    }

    // The width: the brush's, or the marker's (canvas units, its own range).
    if(_marking) {
        rde_ui_slider_set_range(_toolbar->size, FUDE_INK_MARKER_MIN, FUDE_INK_MARKER_MAX);
        rde_ui_slider_set_value(_toolbar->size, _app->ink->marker_radius);
    } else {
        rde_ui_slider_set_range(_toolbar->size, FUDE_TOOLBAR_SIZE_MIN, FUDE_TOOLBAR_SIZE_MAX);
        rde_ui_slider_set_value(_toolbar->size, _app->ink->constant_radius);
    }
}

void fude_toolbar_update(fude_toolbar* _toolbar, b8 _hidden) {
    if(_toolbar->panel == NULL) {
        return;
    }
    if(_hidden != _toolbar->hidden) {
        _toolbar->hidden = _hidden;
        rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->panel), !_hidden);
        if(_hidden) {
            fude_toolbar_set_palette_open(_toolbar, false);
            fude_toolbar_set_paper_open(_toolbar, false);
        }
    }

    // Another canvas opened (or its file loaded): Paper shows that page's.
    if(_toolbar->app->canvas->page.paper != _toolbar->_paper_shown) {
        fude_toolbar_refresh(_toolbar);
    }

    const b8 _can_undo = fude_ink_can_undo(_toolbar->app->ink);
    const b8 _can_redo = fude_ink_can_redo(_toolbar->app->ink);
    if(!_toolbar->_history_shown || _can_undo != _toolbar->_can_undo_shown) {
        fude_kit_set_enabled(_toolbar->undo, _can_undo);
        _toolbar->_can_undo_shown = _can_undo;
    }
    if(!_toolbar->_history_shown || _can_redo != _toolbar->_can_redo_shown) {
        fude_kit_set_enabled(_toolbar->redo, _can_redo);
        _toolbar->_can_redo_shown = _can_redo;
    }
    _toolbar->_history_shown = true;
}

// --- layout ----------------------------------------------------------------------

// Where a pop-up of _size goes beside one of the bar's buttons (_button_at: its
// centre, UI units — kept within the bar, as the strip may have scrolled it out
// of sight): to the side of a vertical bar, above/below a horizontal one —
// whichever side has room.
RDE_INTERNAL rde_vec_2F fude_toolbar_beside(const fude_toolbar* _toolbar, rde_vec_2F _button_at, rde_vec_2F _size) {
    rde_window*      _window   = _toolbar->app->window;
    const rde_vec_2F _screen   = fude_kit_screen_size(_window);
    const rde_vec_2F _panel_bl = { _toolbar->center.x - _toolbar->panel_size.x * 0.5f, _toolbar->center.y - _toolbar->panel_size.y * 0.5f };
    const rde_vec_2F _button   = { rde_math_clamp_f32(_button_at.x, _panel_bl.x, _panel_bl.x + _toolbar->panel_size.x),
                                   rde_math_clamp_f32(_button_at.y, _panel_bl.y, _panel_bl.y + _toolbar->panel_size.y) };
    rde_vec_2F       _center;
    if(_toolbar->vertical) {
        const f32 _right = _panel_bl.x + _toolbar->panel_size.x + FUDE_TOOLBAR_PALETTE_GAP + _size.x * 0.5f;
        const f32 _left  = _panel_bl.x - FUDE_TOOLBAR_PALETTE_GAP - _size.x * 0.5f;
        _center = (rde_vec_2F){ (_right + _size.x * 0.5f <= _screen.x) ? _right : _left, _button.y };
    } else {
        const f32 _below = _panel_bl.y - FUDE_TOOLBAR_PALETTE_GAP - _size.y * 0.5f;
        const f32 _above = _panel_bl.y + _toolbar->panel_size.y + FUDE_TOOLBAR_PALETTE_GAP + _size.y * 0.5f;
        _center = (rde_vec_2F){ _button.x, (_below - _size.y * 0.5f >= 0.0f) ? _below : _above };
    }
    return fude_kit_clamp(_window, _center, _size);
}

RDE_INTERNAL void fude_toolbar_place_palette(fude_toolbar* _toolbar) {
    const f32        _cols = (f32)FUDE_TOOLBAR_SWATCH_COLS;
    const f32        _rows = (f32)((FUDE_TOOLBAR_PALETTE_COUNT + FUDE_TOOLBAR_SWATCH_COLS - 1) / FUDE_TOOLBAR_SWATCH_COLS);
    const rde_vec_2F _size = { _cols * FUDE_TOOLBAR_SWATCH + (_cols - 1.0f) * FUDE_TOOLBAR_SPACING + 2.0f * FUDE_TOOLBAR_PADDING,
                               _rows * FUDE_TOOLBAR_SWATCH + (_rows - 1.0f) * FUDE_TOOLBAR_SPACING + 2.0f * FUDE_TOOLBAR_PADDING };
    _toolbar->palette_center = fude_toolbar_beside(_toolbar, fude_toolbar_center_of(_toolbar->color), _size);
    _toolbar->palette_size   = _size;
    fude_kit_place(rde_ui_image_as_node(_toolbar->palette), _toolbar->palette_center, _size);
    for(u32 _i = 0; _i < FUDE_TOOLBAR_PALETTE_COUNT; _i++) {
        const u32        _col   = _i % FUDE_TOOLBAR_SWATCH_COLS;
        const u32        _row   = _i / FUDE_TOOLBAR_SWATCH_COLS;
        const rde_vec_2F _local = { FUDE_TOOLBAR_PADDING + (f32)_col * (FUDE_TOOLBAR_SWATCH + FUDE_TOOLBAR_SPACING) + FUDE_TOOLBAR_SWATCH * 0.5f,
                                    _size.y - FUDE_TOOLBAR_PADDING - (f32)_row * (FUDE_TOOLBAR_SWATCH + FUDE_TOOLBAR_SPACING) - FUDE_TOOLBAR_SWATCH * 0.5f };
        fude_kit_place(rde_ui_button_as_node(_toolbar->swatches[_i]), _local, (rde_vec_2F){ FUDE_TOOLBAR_SWATCH, FUDE_TOOLBAR_SWATCH });
    }
}

// The paper panel: its choices in a row, beside the Paper button.
RDE_INTERNAL void fude_toolbar_place_paper(fude_toolbar* _toolbar) {
    const f32        _n    = (f32)FUDE_PAPER_COUNT;
    const rde_vec_2F _size = { _n * FUDE_TOOLBAR_CHOICE_W + (_n - 1.0f) * FUDE_TOOLBAR_SPACING + 2.0f * FUDE_TOOLBAR_PADDING,
                               FUDE_TOOLBAR_CHOICE_H + 2.0f * FUDE_TOOLBAR_PADDING };
    _toolbar->paper_center = fude_toolbar_beside(_toolbar, fude_toolbar_center_of(_toolbar->paper), _size);
    _toolbar->paper_size   = _size;
    fude_kit_place(rde_ui_image_as_node(_toolbar->paper_panel), _toolbar->paper_center, _size);
    // In the order they read: dots, lines, squares, nothing.
    static const FUDE_PAPER_ _order[FUDE_PAPER_COUNT] = { FUDE_PAPER_DOTS, FUDE_PAPER_LINES, FUDE_PAPER_SQUARES, FUDE_PAPER_NONE };
    for(u32 _i = 0; _i < FUDE_PAPER_COUNT; _i++) {
        fude_kit_place(rde_ui_button_as_node(_toolbar->paper_choices[_order[_i]]),
                       (rde_vec_2F){ FUDE_TOOLBAR_PADDING + (f32)_i * (FUDE_TOOLBAR_CHOICE_W + FUDE_TOOLBAR_SPACING) + FUDE_TOOLBAR_CHOICE_W * 0.5f, _size.y * 0.5f },
                       (rde_vec_2F){ FUDE_TOOLBAR_CHOICE_W, FUDE_TOOLBAR_CHOICE_H });
    }
}

// Whichever of the bar's panels is open, placed against the bar where it is now.
RDE_INTERNAL void fude_toolbar_place_popups(fude_toolbar* _toolbar) {
    if(_toolbar->palette_open) {
        fude_toolbar_place_palette(_toolbar);
    }
    if(_toolbar->paper_open) {
        fude_toolbar_place_paper(_toolbar);
    }
}

// Along its axis: the grip, then the strip of tools, as long as they need or as
// the screen allows (then the strip scrolls); minimized, just the grip — and at
// _toolbar->center, clamped on screen. Rotation is just this again.
void fude_toolbar_layout(fude_toolbar* _toolbar) {
    const b8 _v = _toolbar->vertical;

    // A separator is a hairline across the bar, with room either side of it.
    const rde_vec_2F _sep  = _v ? (rde_vec_2F){ FUDE_TOOLBAR_BUTTON_W * 0.55f, FUDE_TOOLBAR_SEPARATOR } : (rde_vec_2F){ FUDE_TOOLBAR_SEPARATOR, FUDE_TOOLBAR_BUTTON_H * 0.55f };
    const rde_vec_2F _tool = { FUDE_TOOLBAR_BUTTON_W, FUDE_TOOLBAR_BUTTON_H };
    struct { rde_ui_node* node; rde_vec_2F size; } _items[] = {
        { rde_ui_button_as_node(_toolbar->undo),         _tool },
        { rde_ui_button_as_node(_toolbar->redo),         _tool },
        { rde_ui_image_as_node(_toolbar->separators[0]), _sep },
        { rde_ui_button_as_node(_toolbar->finger),       _tool },
        { rde_ui_button_as_node(_toolbar->draw),         _tool },
        { rde_ui_button_as_node(_toolbar->mark),         _tool },
        { rde_ui_button_as_node(_toolbar->erase),        _tool },
        { rde_ui_button_as_node(_toolbar->lasso_tool),   _tool },
        { rde_ui_button_as_node(_toolbar->clear),        _tool },
        { rde_ui_image_as_node(_toolbar->separators[1]), _sep },
        { rde_ui_slider_as_node(_toolbar->size),         _v ? (rde_vec_2F){ FUDE_TOOLBAR_SLIDER_W, FUDE_TOOLBAR_SLIDER_LEN } : (rde_vec_2F){ FUDE_TOOLBAR_SLIDER_LEN, FUDE_TOOLBAR_SLIDER_W } },
        { rde_ui_image_as_node(_toolbar->separators[2]), _sep },
        { rde_ui_button_as_node(_toolbar->color),        _tool },
        { rde_ui_button_as_node(_toolbar->brush_scale),  _tool },
        { rde_ui_button_as_node(_toolbar->paper),        _tool },
        { rde_ui_image_as_node(_toolbar->separators[3]), _sep },
        { _toolbar->tools[0] != NULL ? rde_ui_button_as_node(_toolbar->tools[0]) : NULL, _tool },   // the app's (FUDE_EXTENSION_TOOLS)
        { _toolbar->tools[1] != NULL ? rde_ui_button_as_node(_toolbar->tools[1]) : NULL, _tool },
        { rde_ui_image_as_node(_toolbar->separators[4]), _sep },
        { rde_ui_button_as_node(_toolbar->rotate),       _tool },
        { rde_ui_button_as_node(_toolbar->reset_view),   _tool },
    };
    // The app's tools only where they are available (Kana's camera: where text
    // can be read from it): elsewhere they, and the hairline after them when none
    // is, are left out of the bar, not shown dead. The hand only on a tablet (a
    // mouse writes on a computer).
    const fude_extension* _ext   = fude_app_ext(_toolbar->app);
    b8                    _shown[FUDE_EXTENSION_TOOLS];
    b8                    _any   = false;
    for(u32 _t = 0; _t < FUDE_EXTENSION_TOOLS; _t++) {
        _shown[_t] = _toolbar->tools[_t] != NULL && (_ext->tools[_t].available == NULL || _ext->tools[_t].available());
        _any       = _any || _shown[_t];
        if(_toolbar->tools[_t] != NULL) {
            rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->tools[_t]), _shown[_t]);
        }
    }
#if defined(RDE_PLATFORM_MOBILE)
    const b8 _hand = true;
#else
    const b8 _hand = false;
#endif
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->separators[4]), _any);
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->finger), _hand);
    u32 _count = 0;
    for(u32 _i = 0; _i < sizeof(_items) / sizeof(_items[0]); _i++) {
        const rde_ui_node* _n    = _items[_i].node;
        b8                 _tool_left_out = false;
        for(u32 _t = 0; _t < FUDE_EXTENSION_TOOLS; _t++) {
            _tool_left_out = _tool_left_out || (_toolbar->tools[_t] != NULL && _n == rde_ui_button_as_node(_toolbar->tools[_t]) && !_shown[_t]);
        }
        if(_n == NULL || _tool_left_out || (!_any && _n == rde_ui_image_as_node(_toolbar->separators[4])) || (!_hand && _n == rde_ui_button_as_node(_toolbar->finger))) {
            continue;
        }
        _items[_count++] = _items[_i];
    }

    // The slider runs along the bar; its thumb has to be wider than its track.
    rde_ui_slider_set_orientation(_toolbar->size, _v ? RDE_UI_ORIENTATION_VERTICAL : RDE_UI_ORIENTATION_HORIZONTAL);
    rde_ui_slider_set_thumb_size(_toolbar->size, _v ? (rde_vec_2F){ 28.0f, 14.0f } : (rde_vec_2F){ 14.0f, 28.0f });

    // The strip's content: every tool at its full size, padding at both ends.
    f32 _along  = 2.0f * FUDE_TOOLBAR_PADDING + FUDE_TOOLBAR_SPACING * (f32)(_count - 1);
    f32 _across = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        _along += _v ? _items[_i].size.y : _items[_i].size.x;
        const f32 _cross = _v ? _items[_i].size.x : _items[_i].size.y;
        _across = _cross > _across ? _cross : _across;
    }
    _across += 2.0f * FUDE_TOOLBAR_PADDING;

    // The bar: the grip, then as much of the strip as the screen has room for.
    const rde_vec_2F _screen = fude_kit_screen_size(_toolbar->app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->app->window);   // left, top, right, bottom
    const f32        _room   = (_v ? _screen.y - (f32)(_insets.y + _insets.w) : _screen.x - (f32)(_insets.x + _insets.z)) - 2.0f * FUDE_KIT_SCREEN_EDGE;
    const b8         _min    = _toolbar->minimized;
    const f32        _grip   = _min ? FUDE_TOOLBAR_MINIMIZED : FUDE_TOOLBAR_PADDING + FUDE_TOOLBAR_GRIP;
    const f32        _strip  = _min ? 0.0f : fmaxf(FUDE_TOOLBAR_BUTTON_H * 2.0f, fminf(_along, _room - _grip));
    _toolbar->panel_size = _v ? (rde_vec_2F){ _across, _grip + _strip } : (rde_vec_2F){ _grip + _strip, _across };
    rde_ui_node_set_active(rde_ui_scroll_area_as_node(_toolbar->strip), !_min);

    // The grip first: the top of a vertical bar, the left of a horizontal one —
    // all of that end, for a finger. The handle drawn in it sits against the
    // strip, or in the middle of a minimized bar.
    const rde_vec_2F _area = _v ? (rde_vec_2F){ _across, _grip } : (rde_vec_2F){ _grip, _across };
    fude_kit_place(rde_ui_image_as_node(_toolbar->grip_area), _v ? (rde_vec_2F){ _across * 0.5f, _strip + _grip * 0.5f } : (rde_vec_2F){ _grip * 0.5f, _across * 0.5f }, _area);
    const f32 _handle = _min ? _grip * 0.5f : FUDE_TOOLBAR_PADDING + FUDE_TOOLBAR_GRIP * 0.5f;   // from the bar's end
    const c8* _dots   = _v ? FUDE_ICON_GRIP_H : FUDE_ICON_GRIP_V;   // the dots across the bar
    const f32 _back   = fude_kit_icon_bearing(_dots) * FUDE_TOOLBAR_GRIP_PX * FUDE_KIT_EM;   // centred (see FUDE_KIT_ICON_BEARINGS)
    rde_ui_label_set_text(_toolbar->grip, _dots);
    fude_kit_place(rde_ui_label_as_node(_toolbar->grip),
                   _v ? (rde_vec_2F){ _across * 0.5f - _back, _grip - _handle } : (rde_vec_2F){ _handle - _back, _across * 0.5f },
                   _v ? (rde_vec_2F){ FUDE_TOOLBAR_BUTTON_W, FUDE_TOOLBAR_GRIP } : (rde_vec_2F){ FUDE_TOOLBAR_GRIP, FUDE_TOOLBAR_BUTTON_H });

    // Then the strip, and the tools in its content (bottom-left origin; the
    // content's top is the strip's top when it has not scrolled).
    fude_kit_place(rde_ui_scroll_area_as_node(_toolbar->strip),
                   _v ? (rde_vec_2F){ _across * 0.5f, _strip * 0.5f } : (rde_vec_2F){ _grip + _strip * 0.5f, _across * 0.5f },
                   _v ? (rde_vec_2F){ _across, _strip } : (rde_vec_2F){ _strip, _across });
    rde_ui_scroll_area_set_content_size(_toolbar->strip, _v ? (rde_vec_2F){ _across, _along } : (rde_vec_2F){ _along, _across });

    // Vertical: top to bottom. Horizontal: left to right.
    f32 _cursor = _v ? _along - FUDE_TOOLBAR_PADDING : FUDE_TOOLBAR_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32  _len = _v ? _items[_i].size.y : _items[_i].size.x;
        rde_vec_2F _c;
        if(_v) {
            _c = (rde_vec_2F){ _across * 0.5f, _cursor - _len * 0.5f };
            _cursor -= _len + FUDE_TOOLBAR_SPACING;
        } else {
            _c = (rde_vec_2F){ _cursor + _len * 0.5f, _across * 0.5f };
            _cursor += _len + FUDE_TOOLBAR_SPACING;
        }
        // A separator takes its room along the bar but draws a hairline in it.
        b8 _separator = false;
        for(u32 _k = 0; _k < FUDE_TOOLBAR_SEPARATORS; _k++) {
            _separator = _separator || _items[_i].node == rde_ui_image_as_node(_toolbar->separators[_k]);
        }
        const rde_vec_2F _hair = _v ? (rde_vec_2F){ _items[_i].size.x, 1.0f } : (rde_vec_2F){ 1.0f, _items[_i].size.y };
        fude_kit_place(_items[_i].node, _c, _separator ? _hair : _items[_i].size);
    }

    _toolbar->center = fude_kit_clamp(_toolbar->app->window, _toolbar->center, _toolbar->panel_size);
    fude_kit_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
    fude_toolbar_place_popups(_toolbar);
}

// Folds the bar down to its grip, or opens it again, with the grip's end staying
// where it is (under the finger that double tapped it) — then clamped on screen,
// so a bar opened near an edge moves back in.
RDE_INTERNAL void fude_toolbar_set_minimized(fude_toolbar* _toolbar, b8 _minimized) {
    const b8         _v    = _toolbar->vertical;
    const rde_vec_2F _c    = _toolbar->center;
    const f32        _edge = _v ? _c.y + _toolbar->panel_size.y * 0.5f : _c.x - _toolbar->panel_size.x * 0.5f;   // top / left

    _toolbar->minimized = _minimized;
    if(_minimized) {
        fude_toolbar_set_palette_open(_toolbar, false);
        fude_toolbar_set_paper_open(_toolbar, false);
    }
    fude_toolbar_layout(_toolbar);   // the new size

    const rde_vec_2F _size = _toolbar->panel_size;
    _toolbar->center = _v ? (rde_vec_2F){ _c.x, _edge - _size.y * 0.5f } : (rde_vec_2F){ _edge + _size.x * 0.5f, _c.y };
    fude_toolbar_layout(_toolbar);   // and there
}

void fude_toolbar_set_placement(fude_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized) {
    _toolbar->vertical  = _vertical;
    _toolbar->center    = _center;
    _toolbar->minimized = _minimized;
    if(_minimized) {
        fude_toolbar_set_palette_open(_toolbar, false);
        fude_toolbar_set_paper_open(_toolbar, false);
    }
    fude_toolbar_layout(_toolbar);
}

void fude_toolbar_set_palette_open(fude_toolbar* _toolbar, b8 _open) {
    if(_open) {
        fude_toolbar_set_paper_open(_toolbar, false);   // one panel at a time
    }
    _toolbar->palette_open = _open;
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->palette), _open);
    if(_open) {
        fude_toolbar_place_palette(_toolbar);
    }
    fude_toolbar_refresh(_toolbar);   // Color shows it open
}

void fude_toolbar_set_paper_open(fude_toolbar* _toolbar, b8 _open) {
    if(_open && _toolbar->palette_open) {
        fude_toolbar_set_palette_open(_toolbar, false);   // one panel at a time
    }
    _toolbar->paper_open = _open;
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->paper_panel), _open);
    if(_open) {
        fude_toolbar_place_paper(_toolbar);
    }
    fude_toolbar_refresh(_toolbar);   // Paper shows it open
}

// Leaving the Lasso tool drops the selection: it would otherwise sit there with
// no way to act on it.
void fude_toolbar_set_tool(fude_toolbar* _toolbar, FUDE_TOOL_ _tool) {
    if(_tool != FUDE_TOOL_LASSO) {
        fude_lasso_clear(_toolbar->app->lasso, _toolbar->app->ink);
    }
    if(_tool != _toolbar->tool) {
        _toolbar->tool_before = _toolbar->tool;
    }
    _toolbar->tool = _tool;
    fude_toolbar_refresh(_toolbar);
}

void fude_toolbar_pen_double_tap(fude_toolbar* _toolbar, u8 _action) {
    switch((RDE_PEN_TAP_ACTION_)_action) {
        case RDE_PEN_TAP_ACTION_SWITCH_ERASER:
            // The eraser, and from it back to what was in hand.
            fude_toolbar_set_tool(_toolbar, _toolbar->tool != FUDE_TOOL_ERASE ? FUDE_TOOL_ERASE
                                          : _toolbar->tool_before != FUDE_TOOL_ERASE ? _toolbar->tool_before : FUDE_TOOL_DRAW);
            break;
        case RDE_PEN_TAP_ACTION_SWITCH_PREVIOUS:
            fude_toolbar_set_tool(_toolbar, _toolbar->tool_before);
            break;
        case RDE_PEN_TAP_ACTION_SHOW_COLOR_PALETTE:
        case RDE_PEN_TAP_ACTION_SHOW_INK_ATTRIBUTES:
        case RDE_PEN_TAP_ACTION_SHOW_CONTEXTUAL_PALETTE:
            fude_toolbar_set_palette_open(_toolbar, !_toolbar->palette_open);   // the colours and the brush
            break;
        default:
            break;   // off, or the system's own shortcut
    }
}

b8 fude_toolbar_hit(const fude_toolbar* _toolbar, rde_vec_2F _ui) {
    // The bar only while it shows: hidden under a screen it keeps its rect, and a
    // press starting there would be swallowed as the bar's. The panels' rects are
    // kept when they are placed, not read back from the UI (the computed rect
    // only updates at the next layout pass).
    return (!_toolbar->hidden && fude_toolbar_contains(_toolbar->center, _toolbar->panel_size, _ui)) ||
           (_toolbar->palette_open && fude_toolbar_contains(_toolbar->palette_center, _toolbar->palette_size, _ui)) ||
           (_toolbar->paper_open && fude_toolbar_contains(_toolbar->paper_center, _toolbar->paper_size, _ui));
}

// --- callbacks ---------------------------------------------------------------------
//
// Each gets the bar; one that changes the page has the UI catch up after it.

#define FUDE_TOOLBAR_CALLBACK(_name) RDE_INTERNAL RDE_UI_EVENT_RESULT_ _name(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data)
#define FUDE_TOOLBAR_SELF            RDE_UNUSED(_node); RDE_UNUSED(_info); fude_toolbar* _toolbar = (fude_toolbar*)_user_data; fude_app* _app = _toolbar->app; RDE_UNUSED(_app)

FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_undo)  { FUDE_TOOLBAR_SELF; fude_ink_undo(_app->ink); fude_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_redo)  { FUDE_TOOLBAR_SELF; fude_ink_redo(_app->ink); fude_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_clear) { FUDE_TOOLBAR_SELF; fude_ink_clear(_app->ink); fude_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }   // undoable: one Undo brings the page back
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_draw)  { FUDE_TOOLBAR_SELF; fude_toolbar_set_tool(_toolbar, FUDE_TOOL_DRAW); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_mark)  { FUDE_TOOLBAR_SELF; fude_toolbar_set_tool(_toolbar, FUDE_TOOL_MARK); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_erase) { FUDE_TOOLBAR_SELF; fude_toolbar_set_tool(_toolbar, FUDE_TOOL_ERASE); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_lasso) { FUDE_TOOLBAR_SELF; fude_toolbar_set_tool(_toolbar, FUDE_TOOL_LASSO); return RDE_UI_EVENT_RESULT_DEFAULT; }
// The hand: one finger writes, or (off) only the pen.
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_finger) { FUDE_TOOLBAR_SELF; fude_app_set_finger_writes(_app, !_app->finger_writes); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_color)  { FUDE_TOOLBAR_SELF; fude_toolbar_set_palette_open(_toolbar, !_toolbar->palette_open); return RDE_UI_EVENT_RESULT_DEFAULT; }
// Paper opens (or closes) its panel.
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_paper)  { FUDE_TOOLBAR_SELF; fude_toolbar_set_paper_open(_toolbar, !_toolbar->paper_open); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_rotate) { FUDE_TOOLBAR_SELF; _toolbar->vertical = !_toolbar->vertical; fude_toolbar_layout(_toolbar); return RDE_UI_EVENT_RESULT_DEFAULT; }
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_reset_view) { FUDE_TOOLBAR_SELF; fude_canvas_reset_view(_app->canvas); return RDE_UI_EVENT_RESULT_DEFAULT; }

FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_brush_scale) {
    FUDE_TOOLBAR_SELF;
    _app->ink->brush_scale = _app->ink->brush_scale == FUDE_INK_BRUSH_SCALE_PAGE ? FUDE_INK_BRUSH_SCALE_SCREEN : FUDE_INK_BRUSH_SCALE_PAGE;
    fude_toolbar_refresh(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// One of the app's tools (its ref: which): the bar's panels closed, then its press.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_toolbar_on_tool(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_toolbar_ref* _ref     = (const fude_toolbar_ref*)_user_data;
    fude_toolbar*           _toolbar = _ref->toolbar;
    fude_app*               _app     = _toolbar->app;
    fude_toolbar_set_palette_open(_toolbar, false);
    fude_toolbar_set_paper_open(_toolbar, false);
    fude_app_ext(_app)->tools[_ref->index].press(_app);
    fude_ui_update(_app->ui);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_swatch) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_toolbar_ref* _ref = (const fude_toolbar_ref*)_user_data;
    // Picking a colour means wanting to write with it: the marker's while it is
    // in hand, else the pen's.
    const b8 _marking = _ref->toolbar->tool == FUDE_TOOL_MARK;
    if(_marking) {
        _ref->toolbar->app->ink->marker_color = FUDE_TOOLBAR_MARKERS[_ref->index];
    } else {
        _ref->toolbar->app->ink->color = FUDE_TOOLBAR_PALETTE[_ref->index];
    }
    fude_toolbar_set_palette_open(_ref->toolbar, false);
    fude_toolbar_set_tool(_ref->toolbar, _marking ? FUDE_TOOL_MARK : FUDE_TOOL_DRAW);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A paper chosen: the page's, saved with it (the size of lines and squares is a
// setting: Settings › Lines & squares).
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_paper_choice) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_toolbar_ref* _ref = (const fude_toolbar_ref*)_user_data;
    _ref->toolbar->app->canvas->page.paper = (FUDE_PAPER_)_ref->index;
    fude_toolbar_set_paper_open(_ref->toolbar, false);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL void fude_toolbar_on_size(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    fude_toolbar* _toolbar = (fude_toolbar*)_user_data;
    if(_toolbar->tool == FUDE_TOOL_MARK) {
        _toolbar->app->ink->marker_radius = rde_ui_slider_get_value(_toolbar->size);
    } else {
        _toolbar->app->ink->constant_radius = rde_ui_slider_get_value(_toolbar->size);
    }
}

// Grip drag. Position = where the bar was + how far the pointer has gone since the
// PRESS (not a sum of deltas: the movement before the drag threshold would be lost
// and the bar would trail the finger).
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_drag_begin) {
    RDE_UNUSED(_node);
    fude_toolbar* _toolbar = (fude_toolbar*)_user_data;
    _toolbar->drag_start_center = _toolbar->center;
    _toolbar->drag_press        = _info->press_position;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_drag_move) {
    RDE_UNUSED(_node);
    fude_toolbar* _toolbar = (fude_toolbar*)_user_data;
    _toolbar->center = (rde_vec_2F){ _toolbar->drag_start_center.x + (_info->position.x - _toolbar->drag_press.x),
                                     _toolbar->drag_start_center.y + (_info->position.y - _toolbar->drag_press.y) };
    _toolbar->center = fude_kit_clamp(_toolbar->app->window, _toolbar->center, _toolbar->panel_size);
    fude_kit_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
    fude_toolbar_place_popups(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap on the grip (a drag is not one: the engine drops the click once the grip
// drags). Two close together, in time and place, fold the bar or open it.
FUDE_TOOLBAR_CALLBACK(fude_toolbar_on_grip_tap) {
    RDE_UNUSED(_node);
    fude_toolbar* _toolbar = (fude_toolbar*)_user_data;
    const f64     _now     = rde_engine_get_time_now();
    const f32     _dx      = _info->position.x - _toolbar->grip_tapped_at.x;
    const f32     _dy      = _info->position.y - _toolbar->grip_tapped_at.y;
    if(_toolbar->grip_tapped > 0.0 && _now - _toolbar->grip_tapped <= FUDE_TOOLBAR_DOUBLE_TAP &&
       _dx * _dx + _dy * _dy <= FUDE_TOOLBAR_DOUBLE_TAP_SLOP * FUDE_TOOLBAR_DOUBLE_TAP_SLOP) {
        _toolbar->grip_tapped = 0.0;   // a third tap starts over
        fude_toolbar_set_minimized(_toolbar, !_toolbar->minimized);
    } else {
        _toolbar->grip_tapped    = _now;
        _toolbar->grip_tapped_at = _info->position;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the theme -------------------------------------------------------------------------

void fude_toolbar_restyle(fude_toolbar* _toolbar) {
    if(_toolbar->panel == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_style_panel(_toolbar->panel, FUDE_TOOLBAR_RADIUS, 1.0f);
    fude_kit_style_panel(_toolbar->palette, 14.0f, 1.0f);
    fude_kit_style_panel(_toolbar->paper_panel, 14.0f, 1.0f);
    rde_ui_image_set_style(_toolbar->grip_area, RDE_UI_STATE_NORMAL, fude_kit_style((rde_color){ 0, 0, 0, 0 }, 0.0f));
    rde_ui_label_set_color(_toolbar->grip, _t->grip);
    for(u32 _i = 0; _i < FUDE_TOOLBAR_SEPARATORS; _i++) {
        rde_ui_image_set_style(_toolbar->separators[_i], RDE_UI_STATE_NORMAL, fude_kit_style(_t->outline, 0.0f));
    }
    rde_ui_slider_set_track_styles(_toolbar->size, RDE_UI_STATE_NORMAL, fude_kit_style(_t->slider_track, 3.0f));
    rde_ui_slider_set_fill_styles(_toolbar->size,  RDE_UI_STATE_NORMAL, fude_kit_style(_t->slider_fill, 3.0f));
    rde_ui_slider_set_thumb_styles(_toolbar->size, RDE_UI_STATE_NORMAL, fude_kit_style(_t->slider_thumb, 7.0f));

    // The tools are quiet; the ones that show a state are set after (refresh).
    rde_ui_button* const _tools[] = {
        _toolbar->undo, _toolbar->redo, _toolbar->finger, _toolbar->draw, _toolbar->mark, _toolbar->erase, _toolbar->lasso_tool, _toolbar->clear,
        _toolbar->brush_scale, _toolbar->paper, _toolbar->rotate, _toolbar->reset_view,
        _toolbar->paper_choices[0], _toolbar->paper_choices[1], _toolbar->paper_choices[2], _toolbar->paper_choices[3],
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        fude_kit_restyle_quiet(_tools[_i]);
    }
    for(u32 _t = 0; _t < FUDE_EXTENSION_TOOLS; _t++) {
        if(_toolbar->tools[_t] != NULL) {
            fude_kit_restyle_quiet(_toolbar->tools[_t]);
        }
    }

    // The strip: no track, a thin thumb in the grip's colour.
    rde_ui_scroll_area_set_background_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_toolbar->strip, _t->grip, fude_kit_shade(_t->grip, 20), fude_kit_shade(_t->grip, 40));

    fude_toolbar_refresh(_toolbar);   // the swatches too: round, ringed, the pen's or the marker's
}

// --- lifetime ------------------------------------------------------------------------

void fude_toolbar_create(fude_toolbar* _toolbar, rde_ui_node* _root, fude_app* _app) {
    _toolbar->app = _app;

    // The panel is what moves; everything else is its child. blocks_input so a
    // press in a gap between buttons still counts as the toolbar's.
    _toolbar->panel = rde_ui_image_create(NULL);
    rde_ui_node* _panel = rde_ui_image_as_node(_toolbar->panel);
    rde_ui_node_set_blocks_input(_panel, true);
    rde_ui_node_add_child(_root, _panel);

    // The grip's end of the bar takes the drags and the taps, not the handle drawn
    // in it: a finger does not have to find the handle. A sibling of the strip, so
    // no tap on a tool ever counts towards a double tap.
    _toolbar->grip_area = rde_ui_image_create(NULL);
    {
        rde_ui_node* _a = rde_ui_image_as_node(_toolbar->grip_area);
        rde_ui_node_set_blocks_input(_a, true);
        rde_ui_node_set_user_data(_a, _toolbar);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, fude_toolbar_on_drag_begin);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  fude_toolbar_on_drag_move);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_CLICK,      fude_toolbar_on_grip_tap);
        rde_ui_node_add_child(_panel, _a);
    }
    _toolbar->grip = rde_ui_label_create(NULL);
    {
        rde_ui_label_set_font(_toolbar->grip, fude_ui_icon_font(_app->ui));
        rde_ui_label_set_font_scale(_toolbar->grip, FUDE_TOOLBAR_GRIP_PX / (f32)FUDE_KIT_FONT_SIZE);
        rde_ui_label_set_alignment(_toolbar->grip, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_node* _g = rde_ui_label_as_node(_toolbar->grip);
        rde_ui_node_set_raycast_target(_g, false);
        rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->grip_area), _g);
    }

    // The tools, in a strip that scrolls along the bar (a drag that starts on a
    // button scrolls it; a tap presses the button): icons, their names kept as
    // labels (hidden).
    _toolbar->strip = rde_ui_scroll_area_create(NULL);
    rde_ui_scroll_area_set_bar_thickness(_toolbar->strip, 3.0f);
    rde_ui_node_add_child(_panel, rde_ui_scroll_area_as_node(_toolbar->strip));
    rde_ui_node* _tools = rde_ui_scroll_area_as_node(_toolbar->strip);
    const struct { rde_ui_button** button; FUDE_TEXT_ text; const c8* icon; rde_ui_event_callback press; } _buttons[] = {
        { &_toolbar->undo,        FUDE_TEXT_UNDO,        FUDE_ICON_UNDO,        fude_toolbar_on_undo },
        { &_toolbar->redo,        FUDE_TEXT_REDO,        FUDE_ICON_REDO,        fude_toolbar_on_redo },
        { &_toolbar->finger,      FUDE_TEXT_TOOL_FINGER, FUDE_ICON_FINGER,      fude_toolbar_on_finger },
        { &_toolbar->draw,        FUDE_TEXT_TOOL_DRAW,   FUDE_ICON_DRAW,        fude_toolbar_on_draw },
        { &_toolbar->mark,        FUDE_TEXT_TOOL_MARK,   FUDE_ICON_MARKER,      fude_toolbar_on_mark },
        { &_toolbar->erase,       FUDE_TEXT_TOOL_ERASE,  FUDE_ICON_ERASE,       fude_toolbar_on_erase },
        { &_toolbar->lasso_tool,  FUDE_TEXT_TOOL_LASSO,  FUDE_ICON_LASSO,       fude_toolbar_on_lasso },
        { &_toolbar->clear,       FUDE_TEXT_TOOL_CLEAR,  FUDE_ICON_TRASH,       fude_toolbar_on_clear },
        { &_toolbar->color,       FUDE_TEXT_TOOL_COLOR,  NULL,                  fude_toolbar_on_color },
        { &_toolbar->brush_scale, FUDE_TEXT_TOOL_PAGE,   FUDE_ICON_PAGE,        fude_toolbar_on_brush_scale },
        { &_toolbar->paper,       FUDE_TEXT_TOOL_PAPER,  FUDE_ICON_PAPER_DOTS,  fude_toolbar_on_paper },
        { &_toolbar->rotate,      FUDE_TEXT_TOOL_ROTATE, FUDE_ICON_ROTATE,      fude_toolbar_on_rotate },
        { &_toolbar->reset_view,  FUDE_TEXT_TOOL_RESET,  FUDE_ICON_RESET_VIEW,  fude_toolbar_on_reset_view },
    };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        *_buttons[_i].button = fude_kit_button(_tools, fude_text(_buttons[_i].text), _buttons[_i].press, _toolbar);
        if(_buttons[_i].icon != NULL) {
            fude_kit_icon(*_buttons[_i].button, _buttons[_i].icon, FUDE_KIT_ICON_ONLY, FUDE_TOOLBAR_TOOL_ICON_PX);
        }
    }
    // The app's own (extension.h).
    const fude_extension* _ext = fude_app_ext(_app);
    for(u32 _t = 0; _ext->tools != NULL && _t < _ext->tool_count && _t < FUDE_EXTENSION_TOOLS; _t++) {
        _toolbar->tool_refs[_t] = (fude_toolbar_ref){ _toolbar, _t };
        _toolbar->tools[_t]     = fude_kit_button(_tools, fude_text((FUDE_TEXT_)_ext->tools[_t].text), fude_toolbar_on_tool, &_toolbar->tool_refs[_t]);
        fude_kit_icon(_toolbar->tools[_t], _ext->tools[_t].icon, FUDE_KIT_ICON_ONLY, FUDE_TOOLBAR_TOOL_ICON_PX);
    }
    for(u32 _i = 0; _i < FUDE_TOOLBAR_SEPARATORS; _i++) {
        _toolbar->separators[_i] = rde_ui_image_create(NULL);
        rde_ui_node_set_raycast_target(rde_ui_image_as_node(_toolbar->separators[_i]), false);
        rde_ui_node_add_child(_tools, rde_ui_image_as_node(_toolbar->separators[_i]));
    }
    _toolbar->size = rde_ui_slider_create(NULL);
    {
        rde_ui_node* _n = rde_ui_slider_as_node(_toolbar->size);
        rde_ui_slider_set_range(_toolbar->size, FUDE_TOOLBAR_SIZE_MIN, FUDE_TOOLBAR_SIZE_MAX);
        rde_ui_slider_set_step(_toolbar->size, 0.5f);
        rde_ui_node_set_user_data(_n, _toolbar);
        rde_ui_slider_set_on_value_changed(_toolbar->size, fude_toolbar_on_size);
        rde_ui_node_add_child(_tools, _n);
    }
    // Color shows the colour itself: a dot in the button.
    rde_ui_button_set_text(_toolbar->color, NULL);
    _toolbar->color_dot = rde_ui_image_create(NULL);
    {
        rde_ui_node* _dot = rde_ui_image_as_node(_toolbar->color_dot);
        rde_ui_node_set_raycast_target(_dot, false);
        rde_ui_node_add_child(rde_ui_button_as_node(_toolbar->color), _dot);
        fude_kit_place(_dot, (rde_vec_2F){ FUDE_TOOLBAR_BUTTON_W * 0.5f, FUDE_TOOLBAR_BUTTON_H * 0.5f }, (rde_vec_2F){ FUDE_TOOLBAR_DOT, FUDE_TOOLBAR_DOT });
    }

    // The palette is its own panel under the root, so it can sit outside the bar.
    _toolbar->palette = rde_ui_image_create(NULL);
    rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->palette), true);
    rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->palette));
    for(u32 _i = 0; _i < FUDE_TOOLBAR_PALETTE_COUNT; _i++) {
        _toolbar->swatch_refs[_i] = (fude_toolbar_ref){ _toolbar, _i };
        _toolbar->swatches[_i]    = rde_ui_button_create(NULL, NULL);
        rde_ui_button_set_on_click(_toolbar->swatches[_i], fude_toolbar_on_swatch, &_toolbar->swatch_refs[_i]);
        rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->palette), rde_ui_button_as_node(_toolbar->swatches[_i]));
    }

    // The paper panel, the same way.
    _toolbar->paper_panel = rde_ui_image_create(NULL);
    rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->paper_panel), true);
    rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->paper_panel));
    static const FUDE_TEXT_ _names[FUDE_PAPER_COUNT] = { FUDE_TEXT_PAPER_DOTS, FUDE_TEXT_PAPER_SQUARES, FUDE_TEXT_PAPER_LINES, FUDE_TEXT_PAPER_NONE };   // FUDE_PAPER_ order
    static const c8* const  _icons[FUDE_PAPER_COUNT] = { FUDE_ICON_PAPER_DOTS, FUDE_ICON_PAPER_SQUARES, FUDE_ICON_PAPER_LINES, FUDE_ICON_PAPER_NONE };
    for(u32 _i = 0; _i < FUDE_PAPER_COUNT; _i++) {
        _toolbar->paper_refs[_i]    = (fude_toolbar_ref){ _toolbar, _i };
        _toolbar->paper_choices[_i] = fude_kit_button(rde_ui_image_as_node(_toolbar->paper_panel), fude_text(_names[_i]), fude_toolbar_on_paper_choice, &_toolbar->paper_refs[_i]);
        fude_kit_icon(_toolbar->paper_choices[_i], _icons[_i], FUDE_KIT_ICON_ABOVE, 18.0f);
    }

    // The first time: on the right edge, vertically centred.
    if(_toolbar->center.x == 0.0f && _toolbar->center.y == 0.0f) {
        const rde_vec_2F _screen = fude_kit_screen_size(_app->window);
        _toolbar->center = (rde_vec_2F){ _screen.x - 60.0f, _screen.y * 0.5f };
    }
    fude_toolbar_layout(_toolbar);
    fude_toolbar_set_palette_open(_toolbar, false);
    fude_toolbar_set_paper_open(_toolbar, false);
}
