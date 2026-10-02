#include "widgets/toolbar.h"
#include "app/app.h"
#include "app/ui.h"
#include "widgets/kit.h"
#include "widgets/icons.h"
#include "base/text.h"
#include "base/theme.h"
#include "services/textscan.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See toolbar.h.
// ===========================================================================

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
#define KANA_TOOLBAR_MINIMIZED   48.0f    // the folded bar's length: just its grip, big enough for a finger
#define KANA_TOOLBAR_DOUBLE_TAP  0.35     // seconds between a double tap's two taps
#define KANA_TOOLBAR_DOUBLE_TAP_SLOP 32.0f   // and how far apart they can be
#define KANA_TOOLBAR_SWATCH      36.0f
#define KANA_TOOLBAR_CHOICE_W    72.0f    // the paper panel's choices: an icon over its name
#define KANA_TOOLBAR_CHOICE_H    60.0f
#define KANA_TOOLBAR_SWATCH_COLS 4u
#define KANA_TOOLBAR_PALETTE_GAP 8.0f

// The palette: every colour but the first is fixed; the first is the theme's ink.
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

RDE_INTERNAL b8 kana_toolbar_contains(rde_vec_2F _center, rde_vec_2F _size, rde_vec_2F _p) {
    return _p.x >= _center.x - _size.x * 0.5f && _p.x <= _center.x + _size.x * 0.5f &&
           _p.y >= _center.y - _size.y * 0.5f && _p.y <= _center.y + _size.y * 0.5f;
}

// A button's centre as laid out last frame (UI units).
RDE_INTERNAL rde_vec_2F kana_toolbar_center_of(rde_ui_button* _button) {
    const rde_ui_node* _n = rde_ui_button_as_node(_button);
    return (rde_vec_2F){ _n->rect.computed_position.x + _n->rect.computed_size.x * 0.5f, _n->rect.computed_position.y + _n->rect.computed_size.y * 0.5f };
}

// --- state → widgets -----------------------------------------------------------

void kana_toolbar_refresh(kana_toolbar* _toolbar) {
    kana_app* _app = _toolbar->app;
    const struct { rde_ui_button* button; KANA_TOOL_ tool; } _tools[] = {
        { _toolbar->draw,       KANA_TOOL_DRAW  },
        { _toolbar->erase,      KANA_TOOL_ERASE },
        { _toolbar->lasso_tool, KANA_TOOL_LASSO },
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        if(_tools[_i].tool == _toolbar->tool) { kana_kit_button_selected(_tools[_i].button); }
        else                                  { kana_kit_button_quiet(_tools[_i].button); }
    }
    // The hand: chosen while a finger writes.
    if(_app->finger_writes) { kana_kit_button_selected(_toolbar->finger); }
    else                    { kana_kit_button_quiet(_toolbar->finger); }

    // Color's dot IS the current colour (the theme's ink, when that is it),
    // ringed so white or the page's own colour still shows.
    rde_ui_style _dot = kana_kit_style(kana_theme_resolve(_app->ink->color), KANA_TOOLBAR_DOT * 0.5f);
    _dot.border_width = 2.0f;
    _dot.border_color = kana_theme_active()->outline;
    rde_ui_image_set_style(_toolbar->color_dot, RDE_UI_STATE_NORMAL, _dot);
    if(_toolbar->palette_open) { kana_kit_button_selected(_toolbar->color); }
    else                       { kana_kit_button_quiet(_toolbar->color); }

    // Page or Screen: what the brush's width keeps to.
    const b8 _page = _app->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE;
    rde_ui_button_set_text(_toolbar->brush_scale, kana_text(_page ? KANA_TEXT_TOOL_PAGE : KANA_TEXT_TOOL_SCREEN));
    kana_kit_icon(_toolbar->brush_scale, _page ? KANA_ICON_PAGE : KANA_ICON_TABLET, KANA_KIT_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);

    // The paper panel shows the page's own paper chosen: it follows the canvas
    // open. Paper shows it too.
    static const c8* const _paper_icons[KANA_PAPER_COUNT] = { KANA_ICON_PAPER_DOTS, KANA_ICON_PAPER_SQUARES, KANA_ICON_PAPER_LINES, KANA_ICON_PAPER_NONE };
    const KANA_PAPER_ _now = _app->canvas->page.paper;
    for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
        if((KANA_PAPER_)_i == _now) { kana_kit_button_selected(_toolbar->paper_choices[_i]); }
        else                        { kana_kit_button_quiet(_toolbar->paper_choices[_i]); }
    }
    kana_kit_icon(_toolbar->paper, _paper_icons[(u32)_now < KANA_PAPER_COUNT ? (u32)_now : 0u], KANA_KIT_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);
    if(_toolbar->paper_open) { kana_kit_button_selected(_toolbar->paper); }
    else                     { kana_kit_button_quiet(_toolbar->paper); }
    _toolbar->_paper_shown = _now;

    rde_ui_slider_set_value(_toolbar->size, _app->ink->constant_radius);
}

void kana_toolbar_update(kana_toolbar* _toolbar, b8 _hidden) {
    if(_toolbar->panel == NULL) {
        return;
    }
    if(_hidden != _toolbar->hidden) {
        _toolbar->hidden = _hidden;
        rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->panel), !_hidden);
        if(_hidden) {
            kana_toolbar_set_palette_open(_toolbar, false);
            kana_toolbar_set_paper_open(_toolbar, false);
        }
    }

    // Another canvas opened (or its file loaded): Paper shows that page's.
    if(_toolbar->app->canvas->page.paper != _toolbar->_paper_shown) {
        kana_toolbar_refresh(_toolbar);
    }

    const b8 _can_undo = kana_ink_can_undo(_toolbar->app->ink);
    const b8 _can_redo = kana_ink_can_redo(_toolbar->app->ink);
    if(!_toolbar->_history_shown || _can_undo != _toolbar->_can_undo_shown) {
        kana_kit_set_enabled(_toolbar->undo, _can_undo);
        _toolbar->_can_undo_shown = _can_undo;
    }
    if(!_toolbar->_history_shown || _can_redo != _toolbar->_can_redo_shown) {
        kana_kit_set_enabled(_toolbar->redo, _can_redo);
        _toolbar->_can_redo_shown = _can_redo;
    }
    _toolbar->_history_shown = true;
}

// --- layout ----------------------------------------------------------------------

// Where a pop-up of _size goes beside one of the bar's buttons (_button_at: its
// centre, UI units — kept within the bar, as the strip may have scrolled it out
// of sight): to the side of a vertical bar, above/below a horizontal one —
// whichever side has room.
RDE_INTERNAL rde_vec_2F kana_toolbar_beside(const kana_toolbar* _toolbar, rde_vec_2F _button_at, rde_vec_2F _size) {
    rde_window*      _window   = _toolbar->app->window;
    const rde_vec_2F _screen   = kana_kit_screen_size(_window);
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
    return kana_kit_clamp(_window, _center, _size);
}

RDE_INTERNAL void kana_toolbar_place_palette(kana_toolbar* _toolbar) {
    const f32        _cols = (f32)KANA_TOOLBAR_SWATCH_COLS;
    const f32        _rows = (f32)((KANA_TOOLBAR_PALETTE_COUNT + KANA_TOOLBAR_SWATCH_COLS - 1) / KANA_TOOLBAR_SWATCH_COLS);
    const rde_vec_2F _size = { _cols * KANA_TOOLBAR_SWATCH + (_cols - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING,
                               _rows * KANA_TOOLBAR_SWATCH + (_rows - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING };
    _toolbar->palette_center = kana_toolbar_beside(_toolbar, kana_toolbar_center_of(_toolbar->color), _size);
    _toolbar->palette_size   = _size;
    kana_kit_place(rde_ui_image_as_node(_toolbar->palette), _toolbar->palette_center, _size);
    for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
        const u32        _col   = _i % KANA_TOOLBAR_SWATCH_COLS;
        const u32        _row   = _i / KANA_TOOLBAR_SWATCH_COLS;
        const rde_vec_2F _local = { KANA_TOOLBAR_PADDING + (f32)_col * (KANA_TOOLBAR_SWATCH + KANA_TOOLBAR_SPACING) + KANA_TOOLBAR_SWATCH * 0.5f,
                                    _size.y - KANA_TOOLBAR_PADDING - (f32)_row * (KANA_TOOLBAR_SWATCH + KANA_TOOLBAR_SPACING) - KANA_TOOLBAR_SWATCH * 0.5f };
        kana_kit_place(rde_ui_button_as_node(_toolbar->swatches[_i]), _local, (rde_vec_2F){ KANA_TOOLBAR_SWATCH, KANA_TOOLBAR_SWATCH });
    }
}

// The paper panel: its choices in a row, beside the Paper button.
RDE_INTERNAL void kana_toolbar_place_paper(kana_toolbar* _toolbar) {
    const f32        _n    = (f32)KANA_PAPER_COUNT;
    const rde_vec_2F _size = { _n * KANA_TOOLBAR_CHOICE_W + (_n - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING,
                               KANA_TOOLBAR_CHOICE_H + 2.0f * KANA_TOOLBAR_PADDING };
    _toolbar->paper_center = kana_toolbar_beside(_toolbar, kana_toolbar_center_of(_toolbar->paper), _size);
    _toolbar->paper_size   = _size;
    kana_kit_place(rde_ui_image_as_node(_toolbar->paper_panel), _toolbar->paper_center, _size);
    // In the order they read: dots, lines, squares, nothing.
    static const KANA_PAPER_ _order[KANA_PAPER_COUNT] = { KANA_PAPER_DOTS, KANA_PAPER_LINES, KANA_PAPER_SQUARES, KANA_PAPER_NONE };
    for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
        kana_kit_place(rde_ui_button_as_node(_toolbar->paper_choices[_order[_i]]),
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

// Along its axis: the grip, then the strip of tools, as long as they need or as
// the screen allows (then the strip scrolls); minimized, just the grip — and at
// _toolbar->center, clamped on screen. Rotation is just this again.
void kana_toolbar_layout(kana_toolbar* _toolbar) {
    const b8 _v = _toolbar->vertical;

    // A separator is a hairline across the bar, with room either side of it.
    const rde_vec_2F _sep  = _v ? (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W * 0.55f, KANA_TOOLBAR_SEPARATOR } : (rde_vec_2F){ KANA_TOOLBAR_SEPARATOR, KANA_TOOLBAR_BUTTON_H * 0.55f };
    const rde_vec_2F _tool = { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H };
    struct { rde_ui_node* node; rde_vec_2F size; } _items[] = {
        { rde_ui_button_as_node(_toolbar->undo),         _tool },
        { rde_ui_button_as_node(_toolbar->redo),         _tool },
        { rde_ui_image_as_node(_toolbar->separators[0]), _sep },
        { rde_ui_button_as_node(_toolbar->finger),       _tool },
        { rde_ui_button_as_node(_toolbar->draw),         _tool },
        { rde_ui_button_as_node(_toolbar->erase),        _tool },
        { rde_ui_button_as_node(_toolbar->lasso_tool),   _tool },
        { rde_ui_button_as_node(_toolbar->clear),        _tool },
        { rde_ui_image_as_node(_toolbar->separators[1]), _sep },
        { rde_ui_slider_as_node(_toolbar->size),         _v ? (rde_vec_2F){ KANA_TOOLBAR_SLIDER_W, KANA_TOOLBAR_SLIDER_LEN } : (rde_vec_2F){ KANA_TOOLBAR_SLIDER_LEN, KANA_TOOLBAR_SLIDER_W } },
        { rde_ui_image_as_node(_toolbar->separators[2]), _sep },
        { rde_ui_button_as_node(_toolbar->color),        _tool },
        { rde_ui_button_as_node(_toolbar->brush_scale),  _tool },
        { rde_ui_button_as_node(_toolbar->paper),        _tool },
        { rde_ui_image_as_node(_toolbar->separators[3]), _sep },
        { rde_ui_button_as_node(_toolbar->camera),       _tool },
        { rde_ui_image_as_node(_toolbar->separators[4]), _sep },
        { rde_ui_button_as_node(_toolbar->rotate),       _tool },
        { rde_ui_button_as_node(_toolbar->reset_view),   _tool },
    };
    // The camera only where text can be read from it (textscan.h): elsewhere it
    // and the hairline after it are left out of the bar, not shown dead. The hand
    // only on a tablet (a mouse writes on a computer).
    const b8 _camera = kana_textscan_available();
#if defined(RDE_PLATFORM_MOBILE)
    const b8 _hand = true;
#else
    const b8 _hand = false;
#endif
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->camera), _camera);
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->separators[4]), _camera);
    rde_ui_node_set_active(rde_ui_button_as_node(_toolbar->finger), _hand);
    u32 _count = 0;
    for(u32 _i = 0; _i < sizeof(_items) / sizeof(_items[0]); _i++) {
        const rde_ui_node* _n = _items[_i].node;
        if((!_camera && (_n == rde_ui_button_as_node(_toolbar->camera) || _n == rde_ui_image_as_node(_toolbar->separators[4]))) ||
           (!_hand && _n == rde_ui_button_as_node(_toolbar->finger))) {
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
    const rde_vec_2F _screen = kana_kit_screen_size(_toolbar->app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->app->window);   // left, top, right, bottom
    const f32        _room   = (_v ? _screen.y - (f32)(_insets.y + _insets.w) : _screen.x - (f32)(_insets.x + _insets.z)) - 2.0f * KANA_KIT_SCREEN_EDGE;
    const b8         _min    = _toolbar->minimized;
    const f32        _grip   = _min ? KANA_TOOLBAR_MINIMIZED : KANA_TOOLBAR_PADDING + KANA_TOOLBAR_GRIP;
    const f32        _strip  = _min ? 0.0f : fmaxf(KANA_TOOLBAR_BUTTON_H * 2.0f, fminf(_along, _room - _grip));
    _toolbar->panel_size = _v ? (rde_vec_2F){ _across, _grip + _strip } : (rde_vec_2F){ _grip + _strip, _across };
    rde_ui_node_set_active(rde_ui_scroll_area_as_node(_toolbar->strip), !_min);

    // The grip first: the top of a vertical bar, the left of a horizontal one —
    // all of that end, for a finger. The handle drawn in it sits against the
    // strip, or in the middle of a minimized bar.
    const rde_vec_2F _area = _v ? (rde_vec_2F){ _across, _grip } : (rde_vec_2F){ _grip, _across };
    kana_kit_place(rde_ui_image_as_node(_toolbar->grip_area), _v ? (rde_vec_2F){ _across * 0.5f, _strip + _grip * 0.5f } : (rde_vec_2F){ _grip * 0.5f, _across * 0.5f }, _area);
    const f32 _handle = _min ? _grip * 0.5f : KANA_TOOLBAR_PADDING + KANA_TOOLBAR_GRIP * 0.5f;   // from the bar's end
    const c8* _dots   = _v ? KANA_ICON_GRIP_H : KANA_ICON_GRIP_V;   // the dots across the bar
    const f32 _back   = kana_kit_icon_bearing(_dots) * KANA_TOOLBAR_GRIP_PX * KANA_KIT_EM;   // centred (see KANA_KIT_ICON_BEARINGS)
    rde_ui_label_set_text(_toolbar->grip, _dots);
    kana_kit_place(rde_ui_label_as_node(_toolbar->grip),
                   _v ? (rde_vec_2F){ _across * 0.5f - _back, _grip - _handle } : (rde_vec_2F){ _handle - _back, _across * 0.5f },
                   _v ? (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_GRIP } : (rde_vec_2F){ KANA_TOOLBAR_GRIP, KANA_TOOLBAR_BUTTON_H });

    // Then the strip, and the tools in its content (bottom-left origin; the
    // content's top is the strip's top when it has not scrolled).
    kana_kit_place(rde_ui_scroll_area_as_node(_toolbar->strip),
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
        kana_kit_place(_items[_i].node, _c, _separator ? _hair : _items[_i].size);
    }

    _toolbar->center = kana_kit_clamp(_toolbar->app->window, _toolbar->center, _toolbar->panel_size);
    kana_kit_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
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

void kana_toolbar_set_paper_open(kana_toolbar* _toolbar, b8 _open) {
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

// Leaving the Lasso tool drops the selection: it would otherwise sit there with
// no way to act on it.
void kana_toolbar_set_tool(kana_toolbar* _toolbar, KANA_TOOL_ _tool) {
    if(_tool != KANA_TOOL_LASSO) {
        kana_lasso_clear(_toolbar->app->lasso, _toolbar->app->ink);
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

b8 kana_toolbar_hit(const kana_toolbar* _toolbar, rde_vec_2F _ui) {
    // The bar only while it shows: hidden under a screen it keeps its rect, and a
    // press starting there would be swallowed as the bar's. The panels' rects are
    // kept when they are placed, not read back from the UI (the computed rect
    // only updates at the next layout pass).
    return (!_toolbar->hidden && kana_toolbar_contains(_toolbar->center, _toolbar->panel_size, _ui)) ||
           (_toolbar->palette_open && kana_toolbar_contains(_toolbar->palette_center, _toolbar->palette_size, _ui)) ||
           (_toolbar->paper_open && kana_toolbar_contains(_toolbar->paper_center, _toolbar->paper_size, _ui));
}

// --- callbacks ---------------------------------------------------------------------
//
// Each gets the bar; one that changes the page has the UI catch up after it.

#define KANA_TOOLBAR_CALLBACK(_name) RDE_INTERNAL RDE_UI_EVENT_RESULT_ _name(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data)
#define KANA_TOOLBAR_SELF            RDE_UNUSED(_node); RDE_UNUSED(_info); kana_toolbar* _toolbar = (kana_toolbar*)_user_data; kana_app* _app = _toolbar->app; RDE_UNUSED(_app)

KANA_TOOLBAR_CALLBACK(kana_toolbar_on_undo)  { KANA_TOOLBAR_SELF; kana_ink_undo(_app->ink); kana_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_redo)  { KANA_TOOLBAR_SELF; kana_ink_redo(_app->ink); kana_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_clear) { KANA_TOOLBAR_SELF; kana_ink_clear(_app->ink); kana_ui_update(_app->ui); return RDE_UI_EVENT_RESULT_DEFAULT; }   // undoable: one Undo brings the page back
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_draw)  { KANA_TOOLBAR_SELF; kana_toolbar_set_tool(_toolbar, KANA_TOOL_DRAW); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_erase) { KANA_TOOLBAR_SELF; kana_toolbar_set_tool(_toolbar, KANA_TOOL_ERASE); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_lasso) { KANA_TOOLBAR_SELF; kana_toolbar_set_tool(_toolbar, KANA_TOOL_LASSO); return RDE_UI_EVENT_RESULT_DEFAULT; }
// The hand: one finger writes, or (off) only the pen.
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_finger) { KANA_TOOLBAR_SELF; kana_app_set_finger_writes(_app, !_app->finger_writes); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_color)  { KANA_TOOLBAR_SELF; kana_toolbar_set_palette_open(_toolbar, !_toolbar->palette_open); return RDE_UI_EVENT_RESULT_DEFAULT; }
// Paper opens (or closes) its panel.
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_paper)  { KANA_TOOLBAR_SELF; kana_toolbar_set_paper_open(_toolbar, !_toolbar->paper_open); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_rotate) { KANA_TOOLBAR_SELF; _toolbar->vertical = !_toolbar->vertical; kana_toolbar_layout(_toolbar); return RDE_UI_EVENT_RESULT_DEFAULT; }
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_reset_view) { KANA_TOOLBAR_SELF; kana_canvas_reset_view(_app->canvas); return RDE_UI_EVENT_RESULT_DEFAULT; }

KANA_TOOLBAR_CALLBACK(kana_toolbar_on_brush_scale) {
    KANA_TOOLBAR_SELF;
    _app->ink->brush_scale = _app->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    kana_toolbar_refresh(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The camera: Text from a photo with the camera live straight away; what is
// written goes to the middle of the page as it is on screen.
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_camera) {
    KANA_TOOLBAR_SELF;
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_set_paper_open(_toolbar, false);
    kana_scan_open(_app->scan, _app->window, kana_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
    kana_scan_camera(_app->scan);
    kana_ui_update(_app->ui);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

KANA_TOOLBAR_CALLBACK(kana_toolbar_on_swatch) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_ref* _ref = (const kana_toolbar_ref*)_user_data;
    _ref->toolbar->app->ink->color = KANA_TOOLBAR_PALETTE[_ref->index];
    // Picking a colour means wanting to write with it.
    kana_toolbar_set_palette_open(_ref->toolbar, false);
    kana_toolbar_set_tool(_ref->toolbar, KANA_TOOL_DRAW);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// A paper chosen: the page's, saved with it (the size of lines and squares is a
// setting: Settings › Lines & squares).
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_paper_choice) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_toolbar_ref* _ref = (const kana_toolbar_ref*)_user_data;
    _ref->toolbar->app->canvas->page.paper = (KANA_PAPER_)_ref->index;
    kana_toolbar_set_paper_open(_ref->toolbar, false);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL void kana_toolbar_on_size(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->app->ink->constant_radius = rde_ui_slider_get_value(_toolbar->size);
}

// Grip drag. Position = where the bar was + how far the pointer has gone since the
// PRESS (not a sum of deltas: the movement before the drag threshold would be lost
// and the bar would trail the finger).
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_drag_begin) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->drag_start_center = _toolbar->center;
    _toolbar->drag_press        = _info->press_position;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

KANA_TOOLBAR_CALLBACK(kana_toolbar_on_drag_move) {
    RDE_UNUSED(_node);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->center = (rde_vec_2F){ _toolbar->drag_start_center.x + (_info->position.x - _toolbar->drag_press.x),
                                     _toolbar->drag_start_center.y + (_info->position.y - _toolbar->drag_press.y) };
    _toolbar->center = kana_kit_clamp(_toolbar->app->window, _toolbar->center, _toolbar->panel_size);
    kana_kit_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place_popups(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A tap on the grip (a drag is not one: the engine drops the click once the grip
// drags). Two close together, in time and place, fold the bar or open it.
KANA_TOOLBAR_CALLBACK(kana_toolbar_on_grip_tap) {
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

// --- the theme -------------------------------------------------------------------------

void kana_toolbar_restyle(kana_toolbar* _toolbar) {
    if(_toolbar->panel == NULL) {
        return;
    }
    const kana_theme* _t = kana_theme_active();
    kana_kit_style_panel(_toolbar->panel, KANA_TOOLBAR_RADIUS, 1.0f);
    kana_kit_style_panel(_toolbar->palette, 14.0f, 1.0f);
    kana_kit_style_panel(_toolbar->paper_panel, 14.0f, 1.0f);
    rde_ui_image_set_style(_toolbar->grip_area, RDE_UI_STATE_NORMAL, kana_kit_style((rde_color){ 0, 0, 0, 0 }, 0.0f));
    rde_ui_label_set_color(_toolbar->grip, _t->grip);
    for(u32 _i = 0; _i < KANA_TOOLBAR_SEPARATORS; _i++) {
        rde_ui_image_set_style(_toolbar->separators[_i], RDE_UI_STATE_NORMAL, kana_kit_style(_t->outline, 0.0f));
    }
    rde_ui_slider_set_track_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_kit_style(_t->slider_track, 3.0f));
    rde_ui_slider_set_fill_styles(_toolbar->size,  RDE_UI_STATE_NORMAL, kana_kit_style(_t->slider_fill, 3.0f));
    rde_ui_slider_set_thumb_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_kit_style(_t->slider_thumb, 7.0f));

    // The tools are quiet; the ones that show a state are set after (refresh).
    rde_ui_button* const _tools[] = {
        _toolbar->undo, _toolbar->redo, _toolbar->finger, _toolbar->draw, _toolbar->erase, _toolbar->lasso_tool, _toolbar->clear,
        _toolbar->brush_scale, _toolbar->paper, _toolbar->rotate, _toolbar->reset_view, _toolbar->camera,
        _toolbar->paper_choices[0], _toolbar->paper_choices[1], _toolbar->paper_choices[2], _toolbar->paper_choices[3],
    };
    for(u32 _i = 0; _i < sizeof(_tools) / sizeof(_tools[0]); _i++) {
        kana_kit_restyle_quiet(_tools[_i]);
    }

    // The strip: no track, a thin thumb in the grip's colour.
    rde_ui_scroll_area_set_background_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_track_color(_toolbar->strip, (rde_color){ 0, 0, 0, 0 });
    rde_ui_scroll_area_set_thumb_colors(_toolbar->strip, _t->grip, kana_kit_shade(_t->grip, 20), kana_kit_shade(_t->grip, 40));

    // The swatches: round, ringed.
    for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
        kana_kit_button_colors(_toolbar->swatches[_i], kana_theme_resolve(KANA_TOOLBAR_PALETTE[_i]), 2.0f, _t->outline);
        kana_kit_button_round(_toolbar->swatches[_i], KANA_TOOLBAR_SWATCH * 0.5f);
    }
    kana_toolbar_refresh(_toolbar);
}

// --- lifetime ------------------------------------------------------------------------

void kana_toolbar_create(kana_toolbar* _toolbar, rde_ui_node* _root, kana_app* _app) {
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
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, kana_toolbar_on_drag_begin);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  kana_toolbar_on_drag_move);
        rde_ui_node_set_callback(_a, RDE_UI_EVENT_MOUSE_CLICK,      kana_toolbar_on_grip_tap);
        rde_ui_node_add_child(_panel, _a);
    }
    _toolbar->grip = rde_ui_label_create(NULL);
    {
        rde_ui_label_set_font(_toolbar->grip, kana_ui_icon_font(_app->ui));
        rde_ui_label_set_font_scale(_toolbar->grip, KANA_TOOLBAR_GRIP_PX / (f32)KANA_KIT_FONT_SIZE);
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
    const struct { rde_ui_button** button; KANA_TEXT_ text; const c8* icon; rde_ui_event_callback press; } _buttons[] = {
        { &_toolbar->undo,        KANA_TEXT_UNDO,        KANA_ICON_UNDO,        kana_toolbar_on_undo },
        { &_toolbar->redo,        KANA_TEXT_REDO,        KANA_ICON_REDO,        kana_toolbar_on_redo },
        { &_toolbar->finger,      KANA_TEXT_TOOL_FINGER, KANA_ICON_FINGER,      kana_toolbar_on_finger },
        { &_toolbar->draw,        KANA_TEXT_TOOL_DRAW,   KANA_ICON_DRAW,        kana_toolbar_on_draw },
        { &_toolbar->erase,       KANA_TEXT_TOOL_ERASE,  KANA_ICON_ERASE,       kana_toolbar_on_erase },
        { &_toolbar->lasso_tool,  KANA_TEXT_TOOL_LASSO,  KANA_ICON_LASSO,       kana_toolbar_on_lasso },
        { &_toolbar->clear,       KANA_TEXT_TOOL_CLEAR,  KANA_ICON_TRASH,       kana_toolbar_on_clear },
        { &_toolbar->color,       KANA_TEXT_TOOL_COLOR,  NULL,                  kana_toolbar_on_color },
        { &_toolbar->brush_scale, KANA_TEXT_TOOL_PAGE,   KANA_ICON_PAGE,        kana_toolbar_on_brush_scale },
        { &_toolbar->paper,       KANA_TEXT_TOOL_PAPER,  KANA_ICON_PAPER_DOTS,  kana_toolbar_on_paper },
        { &_toolbar->rotate,      KANA_TEXT_TOOL_ROTATE, KANA_ICON_ROTATE,      kana_toolbar_on_rotate },
        { &_toolbar->reset_view,  KANA_TEXT_TOOL_RESET,  KANA_ICON_RESET_VIEW,  kana_toolbar_on_reset_view },
        { &_toolbar->camera,      KANA_TEXT_SCAN_CAMERA, KANA_ICON_CAMERA,      kana_toolbar_on_camera },
    };
    for(u32 _i = 0; _i < sizeof(_buttons) / sizeof(_buttons[0]); _i++) {
        *_buttons[_i].button = kana_kit_button(_tools, kana_text(_buttons[_i].text), _buttons[_i].press, _toolbar);
        if(_buttons[_i].icon != NULL) {
            kana_kit_icon(*_buttons[_i].button, _buttons[_i].icon, KANA_KIT_ICON_ONLY, KANA_TOOLBAR_TOOL_ICON_PX);
        }
    }
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
    // Color shows the colour itself: a dot in the button.
    rde_ui_button_set_text(_toolbar->color, NULL);
    _toolbar->color_dot = rde_ui_image_create(NULL);
    {
        rde_ui_node* _dot = rde_ui_image_as_node(_toolbar->color_dot);
        rde_ui_node_set_raycast_target(_dot, false);
        rde_ui_node_add_child(rde_ui_button_as_node(_toolbar->color), _dot);
        kana_kit_place(_dot, (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W * 0.5f, KANA_TOOLBAR_BUTTON_H * 0.5f }, (rde_vec_2F){ KANA_TOOLBAR_DOT, KANA_TOOLBAR_DOT });
    }

    // The palette is its own panel under the root, so it can sit outside the bar.
    _toolbar->palette = rde_ui_image_create(NULL);
    rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->palette), true);
    rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->palette));
    for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
        _toolbar->swatch_refs[_i] = (kana_toolbar_ref){ _toolbar, _i };
        _toolbar->swatches[_i]    = rde_ui_button_create(NULL, NULL);
        rde_ui_button_set_on_click(_toolbar->swatches[_i], kana_toolbar_on_swatch, &_toolbar->swatch_refs[_i]);
        rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->palette), rde_ui_button_as_node(_toolbar->swatches[_i]));
    }

    // The paper panel, the same way.
    _toolbar->paper_panel = rde_ui_image_create(NULL);
    rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->paper_panel), true);
    rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->paper_panel));
    static const KANA_TEXT_ _names[KANA_PAPER_COUNT] = { KANA_TEXT_PAPER_DOTS, KANA_TEXT_PAPER_SQUARES, KANA_TEXT_PAPER_LINES, KANA_TEXT_PAPER_NONE };   // KANA_PAPER_ order
    static const c8* const  _icons[KANA_PAPER_COUNT] = { KANA_ICON_PAPER_DOTS, KANA_ICON_PAPER_SQUARES, KANA_ICON_PAPER_LINES, KANA_ICON_PAPER_NONE };
    for(u32 _i = 0; _i < KANA_PAPER_COUNT; _i++) {
        _toolbar->paper_refs[_i]    = (kana_toolbar_ref){ _toolbar, _i };
        _toolbar->paper_choices[_i] = kana_kit_button(rde_ui_image_as_node(_toolbar->paper_panel), kana_text(_names[_i]), kana_toolbar_on_paper_choice, &_toolbar->paper_refs[_i]);
        kana_kit_icon(_toolbar->paper_choices[_i], _icons[_i], KANA_KIT_ICON_ABOVE, 18.0f);
    }

    // The first time: on the right edge, vertically centred.
    if(_toolbar->center.x == 0.0f && _toolbar->center.y == 0.0f) {
        const rde_vec_2F _screen = kana_kit_screen_size(_app->window);
        _toolbar->center = (rde_vec_2F){ _screen.x - 60.0f, _screen.y * 0.5f };
    }
    kana_toolbar_layout(_toolbar);
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_set_paper_open(_toolbar, false);
}
