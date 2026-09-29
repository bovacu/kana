#include "toolbar.h"

#include <string.h>

// ===========================================================================
// See toolbar.h.
// ===========================================================================

#define KANA_TOOLBAR_FONT_PATH   "assets/fonts/Roboto-Regular.ttf"
#define KANA_TOOLBAR_FONT_SIZE   32
#define KANA_TOOLBAR_TEXT_SCALE  0.5f     // ~16 units of text from the 32 px font

// Geometry, UI canvas units (= window units: the canvas is CONSTANT_PIXEL).
#define KANA_TOOLBAR_BUTTON_W    64.0f
#define KANA_TOOLBAR_BUTTON_H    44.0f
#define KANA_TOOLBAR_GRIP        18.0f
#define KANA_TOOLBAR_SLIDER_LEN  120.0f
#define KANA_TOOLBAR_SLIDER_W    20.0f
#define KANA_TOOLBAR_SPACING     6.0f
#define KANA_TOOLBAR_PADDING     8.0f
#define KANA_TOOLBAR_SCREEN_EDGE 8.0f     // the bar is kept at least this far inside the screen

#define KANA_TOOLBAR_SWATCH      36.0f
#define KANA_TOOLBAR_SWATCH_COLS 4u
#define KANA_TOOLBAR_PALETTE_GAP 8.0f

#define KANA_TOOLBAR_PANEL_COLOR   (rde_color){  34,  34,  40, 235 }
#define KANA_TOOLBAR_BORDER_COLOR  (rde_color){  72,  72,  84, 255 }
#define KANA_TOOLBAR_GRIP_COLOR    (rde_color){  86,  86, 100, 255 }
#define KANA_TOOLBAR_TEXT_COLOR    (rde_color){ 235, 235, 240, 255 }

RDE_INTERNAL const rde_color KANA_TOOLBAR_PALETTE[KANA_TOOLBAR_PALETTE_COUNT] = {
    {  30,  30,  36, 255 },   // the original ink
    { 240, 240, 245, 255 },
    { 230,  72,  72, 255 },
    { 240, 150,  50, 255 },
    { 240, 210,  70, 255 },
    {  90, 190, 110, 255 },
    {  80, 140, 235, 255 },
    { 170, 110, 220, 255 },
};

// --- styles -------------------------------------------------------------------

RDE_INTERNAL rde_ui_style kana_toolbar_style(rde_color _tint, f32 _radius) {
    rde_ui_style _s  = rde_ui_style_default();
    _s.tint          = _tint;
    _s.corner_radius = _radius;
    return _s;
}

RDE_INTERNAL rde_color kana_toolbar_shade(rde_color _c, i32 _delta) {
    const i32 _r = (i32)_c.r + _delta;
    const i32 _g = (i32)_c.g + _delta;
    const i32 _b = (i32)_c.b + _delta;
    return (rde_color){ (u8)(_r < 0 ? 0 : (_r > 255 ? 255 : _r)),
                        (u8)(_g < 0 ? 0 : (_g > 255 ? 255 : _g)),
                        (u8)(_b < 0 ? 0 : (_b > 255 ? 255 : _b)), _c.a };
}

// Normal / hovered / pressed from one base colour, with an optional border.
RDE_INTERNAL void kana_toolbar_button_colors(rde_ui_button* _button, rde_color _base, f32 _border_width, rde_color _border) {
    rde_ui_style _s  = kana_toolbar_style(_base, 10.0f);
    _s.border_width  = _border_width;
    _s.border_color  = _border;
    rde_ui_button_set_style(_button, RDE_UI_STATE_NORMAL, _s);
    _s.tint = kana_toolbar_shade(_base, 14);
    rde_ui_button_set_style(_button, RDE_UI_STATE_HOVERED, _s);
    _s.tint = kana_toolbar_shade(_base, 32);
    rde_ui_button_set_style(_button, RDE_UI_STATE_PRESSED, _s);
}

RDE_INTERNAL void kana_toolbar_button_plain(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, (rde_color){ 54, 54, 62, 255 }, 0.0f, (rde_color){ 0, 0, 0, 0 });
}

RDE_INTERNAL void kana_toolbar_button_selected(rde_ui_button* _button) {
    kana_toolbar_button_colors(_button, (rde_color){ 58, 108, 200, 255 }, 0.0f, (rde_color){ 0, 0, 0, 0 });
}

RDE_INTERNAL rde_color kana_toolbar_text_on(rde_color _background) {
    const f32 _luma = 0.299f * (f32)_background.r + 0.587f * (f32)_background.g + 0.114f * (f32)_background.b;
    return _luma > 140.0f ? (rde_color){ 20, 20, 24, 255 } : KANA_TOOLBAR_TEXT_COLOR;
}

// --- creation helpers ----------------------------------------------------------

// Every child is pinned bottom-left to its parent with a centre pivot, so
// set_position places its CENTRE in parent-local units (bottom-left origin).
RDE_INTERNAL void kana_toolbar_place(rde_ui_node* _node, rde_vec_2F _center, rde_vec_2F _size) {
    rde_ui_node_set_anchor_preset(_node, RDE_UI_ANCHOR_PRESET_BOTTOM_LEFT);
    rde_ui_node_set_pivot(_node, (rde_vec_2F){ 0.5f, 0.5f });
    rde_ui_node_set_size(_node, _size);
    rde_ui_node_set_position(_node, _center);
}

RDE_INTERNAL rde_ui_button* kana_toolbar_button(kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _text, rde_ui_event_callback _on_click) {
    rde_ui_button* _button = rde_ui_button_create(_toolbar->font, NULL);
    rde_ui_button_set_text(_button, _text);
    kana_toolbar_button_plain(_button);

    // Fit the text, never truncate it: shrink a long word into the button.
    rde_ui_label_set_font_scale(_button->internal_label, KANA_TOOLBAR_TEXT_SCALE);
    rde_ui_label_set_auto_fit(_button->internal_label, true);
    rde_ui_label_set_auto_fit_min_scale(_button->internal_label, 0.3f);
    rde_ui_label_set_color(_button->internal_label, KANA_TOOLBAR_TEXT_COLOR);

    rde_ui_button_set_on_click(_button, _on_click, _toolbar);
    rde_ui_node_add_child(_parent, rde_ui_button_as_node(_button));
    return _button;
}

// --- state → widgets -----------------------------------------------------------

RDE_INTERNAL void kana_toolbar_refresh(kana_toolbar* _toolbar) {
    if(_toolbar->tool == KANA_TOOL_DRAW) {
        kana_toolbar_button_selected(_toolbar->draw);
        kana_toolbar_button_plain(_toolbar->erase);
    } else {
        kana_toolbar_button_plain(_toolbar->draw);
        kana_toolbar_button_selected(_toolbar->erase);
    }

    // The colour button IS the current colour.
    kana_toolbar_button_colors(_toolbar->color, _toolbar->ink->color, 2.0f, (rde_color){ 200, 200, 210, 255 });
    rde_ui_label_set_color(_toolbar->color->internal_label, kana_toolbar_text_on(_toolbar->ink->color));

    rde_ui_button_set_text(_toolbar->brush_scale, _toolbar->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? "Page" : "Screen");
    rde_ui_button_set_text(_toolbar->hud, (_toolbar->show_hud != NULL && *_toolbar->show_hud) ? "HUD on" : "HUD off");

    rde_ui_slider_set_value(_toolbar->size, _toolbar->ink->constant_radius);
}

void kana_toolbar_sync(kana_toolbar* _toolbar) {
    kana_toolbar_refresh(_toolbar);
}

// --- layout ----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F kana_toolbar_virtual_size(const kana_toolbar* _toolbar) {
    const rde_vec_2I _w = rde_window_get_size(_toolbar->window);
    return (rde_vec_2F){ (f32)_w.x, (f32)_w.y };
}

// Keeps a rect of _size centred at _center inside the SAFE AREA — clear of the
// status bar, notch and home indicator. Done here rather than by the canvas: see
// the safe-area note in kana_toolbar_init.
RDE_INTERNAL rde_vec_2F kana_toolbar_clamp(const kana_toolbar* _toolbar, rde_vec_2F _center, rde_vec_2F _size) {
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

RDE_INTERNAL void kana_toolbar_place_palette(kana_toolbar* _toolbar) {
    const f32        _cols   = (f32)KANA_TOOLBAR_SWATCH_COLS;
    const f32        _rows   = (f32)((KANA_TOOLBAR_PALETTE_COUNT + KANA_TOOLBAR_SWATCH_COLS - 1) / KANA_TOOLBAR_SWATCH_COLS);
    const rde_vec_2F _size   = { _cols * KANA_TOOLBAR_SWATCH + (_cols - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING,
                                 _rows * KANA_TOOLBAR_SWATCH + (_rows - 1.0f) * KANA_TOOLBAR_SPACING + 2.0f * KANA_TOOLBAR_PADDING };
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);

    // Beside the colour button: to the side of a vertical bar, above/below a
    // horizontal one — whichever side has room.
    const rde_vec_2F _panel_bl = { _toolbar->center.x - _toolbar->panel_size.x * 0.5f, _toolbar->center.y - _toolbar->panel_size.y * 0.5f };
    const rde_vec_2F _button   = { _panel_bl.x + _toolbar->color_button_center.x, _panel_bl.y + _toolbar->color_button_center.y };
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

    _center = kana_toolbar_clamp(_toolbar, _center, _size);
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

// Lays the children out along the bar's axis, sizes the panel to them, and puts
// the panel at _toolbar->center (clamped on screen). Rotation is just this again.
RDE_INTERNAL void kana_toolbar_layout(kana_toolbar* _toolbar) {
    const b8 _v = _toolbar->vertical;

    struct { rde_ui_node* node; rde_vec_2F size; } _items[] = {
        { rde_ui_image_as_node(_toolbar->grip),       _v ? (rde_vec_2F){ KANA_TOOLBAR_BUTTON_W * 0.6f, KANA_TOOLBAR_GRIP } : (rde_vec_2F){ KANA_TOOLBAR_GRIP, KANA_TOOLBAR_BUTTON_H * 0.6f } },
        { rde_ui_button_as_node(_toolbar->draw),      { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->erase),     { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->clear),     { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_slider_as_node(_toolbar->size),      _v ? (rde_vec_2F){ KANA_TOOLBAR_SLIDER_W, KANA_TOOLBAR_SLIDER_LEN } : (rde_vec_2F){ KANA_TOOLBAR_SLIDER_LEN, KANA_TOOLBAR_SLIDER_W } },
        { rde_ui_button_as_node(_toolbar->color),     { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->brush_scale), { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->rotate),    { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->reset_view), { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
        { rde_ui_button_as_node(_toolbar->hud),       { KANA_TOOLBAR_BUTTON_W, KANA_TOOLBAR_BUTTON_H } },
    };
    const u32 _count = sizeof(_items) / sizeof(_items[0]);

    // The slider runs along the bar; its thumb has to be wider than its track.
    rde_ui_slider_set_orientation(_toolbar->size, _v ? RDE_UI_ORIENTATION_VERTICAL : RDE_UI_ORIENTATION_HORIZONTAL);
    rde_ui_slider_set_thumb_size(_toolbar->size, _v ? (rde_vec_2F){ 28.0f, 14.0f } : (rde_vec_2F){ 14.0f, 28.0f });

    // Panel size: the items' lengths along the axis, the widest item across it.
    f32 _along  = 2.0f * KANA_TOOLBAR_PADDING + KANA_TOOLBAR_SPACING * (f32)(_count - 1);
    f32 _across = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        _along  += _v ? _items[_i].size.y : _items[_i].size.x;
        const f32 _cross = _v ? _items[_i].size.x : _items[_i].size.y;
        _across = _cross > _across ? _cross : _across;
    }
    _across += 2.0f * KANA_TOOLBAR_PADDING;
    _toolbar->panel_size = _v ? (rde_vec_2F){ _across, _along } : (rde_vec_2F){ _along, _across };

    // Vertical: top to bottom. Horizontal: left to right.
    f32 _cursor = _v ? _toolbar->panel_size.y - KANA_TOOLBAR_PADDING : KANA_TOOLBAR_PADDING;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32  _len = _v ? _items[_i].size.y : _items[_i].size.x;
        rde_vec_2F _c;

        if(_v) {
            _c = (rde_vec_2F){ _toolbar->panel_size.x * 0.5f, _cursor - _len * 0.5f };
            _cursor -= _len + KANA_TOOLBAR_SPACING;
        } else {
            _c = (rde_vec_2F){ _cursor + _len * 0.5f, _toolbar->panel_size.y * 0.5f };
            _cursor += _len + KANA_TOOLBAR_SPACING;
        }

        kana_toolbar_place(_items[_i].node, _c, _items[_i].size);

        if(_items[_i].node == rde_ui_button_as_node(_toolbar->color)) {
            _toolbar->color_button_center = _c;
        }
    }

    _toolbar->center = kana_toolbar_clamp(_toolbar, _toolbar->center, _toolbar->panel_size);
    kana_toolbar_place(rde_ui_image_as_node(_toolbar->panel), _toolbar->center, _toolbar->panel_size);

    if(_toolbar->palette_open) {
        kana_toolbar_place_palette(_toolbar);
    }
}

// --- callbacks ---------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_draw(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->tool = KANA_TOOL_DRAW;
    kana_toolbar_refresh(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_erase(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    _toolbar->tool = KANA_TOOL_ERASE;
    kana_toolbar_refresh(_toolbar);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_clear(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_ink_clear(((kana_toolbar*)_user_data)->ink);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

RDE_INTERNAL void kana_toolbar_set_palette_open(kana_toolbar* _toolbar, b8 _open) {
    _toolbar->palette_open = _open;
    rde_ui_node_set_active(rde_ui_image_as_node(_toolbar->palette), _open);
    if(_open) {
        kana_toolbar_place_palette(_toolbar);
    }
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
    _ref->toolbar->tool = KANA_TOOL_DRAW;
    kana_toolbar_set_palette_open(_ref->toolbar, false);
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

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_toolbar_on_hud(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar* _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->show_hud != NULL) {
        *_toolbar->show_hud = !*_toolbar->show_hud;
    }
    kana_toolbar_refresh(_toolbar);
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

    if(_toolbar->palette_open) {
        kana_toolbar_place_palette(_toolbar);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- lifetime ------------------------------------------------------------------------

void kana_toolbar_init(kana_toolbar* _toolbar, rde_window* _window, kana_ink* _ink, kana_canvas* _view, b8* _show_hud) {
    memset(_toolbar, 0, sizeof(*_toolbar));
    _toolbar->window   = _window;
    _toolbar->ink      = _ink;
    _toolbar->view     = _view;
    _toolbar->show_hud = _show_hud;
    _toolbar->tool     = KANA_TOOL_DRAW;
    _toolbar->vertical = true;

    rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _toolbar->font = rde_font_load(KANA_TOOLBAR_FONT_PATH, KANA_TOOLBAR_FONT_SIZE, NULL, &_slug, NULL);

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
    _toolbar->panel = rde_ui_image_create(NULL);
    {
        rde_ui_style _s = kana_toolbar_style(KANA_TOOLBAR_PANEL_COLOR, 14.0f);
        _s.border_width = 1.0f;
        _s.border_color = KANA_TOOLBAR_BORDER_COLOR;
        rde_ui_image_set_style(_toolbar->panel, RDE_UI_STATE_NORMAL, _s);
        rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->panel), true);
        rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->panel));
    }
    rde_ui_node* _panel = rde_ui_image_as_node(_toolbar->panel);

    _toolbar->grip = rde_ui_image_create(NULL);
    {
        rde_ui_image_set_style(_toolbar->grip, RDE_UI_STATE_NORMAL, kana_toolbar_style(KANA_TOOLBAR_GRIP_COLOR, 6.0f));
        rde_ui_node* _g = rde_ui_image_as_node(_toolbar->grip);
        rde_ui_node_set_blocks_input(_g, true);
        rde_ui_node_set_user_data(_g, _toolbar);
        rde_ui_node_set_callback(_g, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, kana_toolbar_on_drag_begin);
        rde_ui_node_set_callback(_g, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  kana_toolbar_on_drag_move);
        rde_ui_node_add_child(_panel, _g);
    }

    _toolbar->draw  = kana_toolbar_button(_toolbar, _panel, "Draw",  kana_toolbar_on_draw);
    _toolbar->erase = kana_toolbar_button(_toolbar, _panel, "Erase", kana_toolbar_on_erase);
    _toolbar->clear = kana_toolbar_button(_toolbar, _panel, "Clear", kana_toolbar_on_clear);

    _toolbar->size = rde_ui_slider_create(NULL);
    {
        rde_ui_node* _n = rde_ui_slider_as_node(_toolbar->size);
        rde_ui_slider_set_range(_toolbar->size, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
        rde_ui_slider_set_step(_toolbar->size, 0.5f);
        rde_ui_slider_set_track_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_toolbar_style((rde_color){  70,  70,  82, 255 }, 4.0f));
        rde_ui_slider_set_fill_styles(_toolbar->size,  RDE_UI_STATE_NORMAL, kana_toolbar_style((rde_color){  90, 135, 220, 255 }, 4.0f));
        rde_ui_slider_set_thumb_styles(_toolbar->size, RDE_UI_STATE_NORMAL, kana_toolbar_style((rde_color){ 230, 230, 236, 255 }, 5.0f));
        rde_ui_node_set_user_data(_n, _toolbar);
        rde_ui_slider_set_on_value_changed(_toolbar->size, kana_toolbar_on_size);
        rde_ui_node_add_child(_panel, _n);
    }

    _toolbar->color       = kana_toolbar_button(_toolbar, _panel, "Color",  kana_toolbar_on_color);
    _toolbar->brush_scale = kana_toolbar_button(_toolbar, _panel, "Page",   kana_toolbar_on_brush_scale);
    _toolbar->rotate      = kana_toolbar_button(_toolbar, _panel, "Rotate", kana_toolbar_on_rotate);
    _toolbar->reset_view  = kana_toolbar_button(_toolbar, _panel, "Reset",  kana_toolbar_on_reset_view);
    _toolbar->hud         = kana_toolbar_button(_toolbar, _panel, "HUD",    kana_toolbar_on_hud);

    // The palette is its own panel under the root, so it can sit outside the bar.
    _toolbar->palette = rde_ui_image_create(NULL);
    {
        rde_ui_style _s = kana_toolbar_style(KANA_TOOLBAR_PANEL_COLOR, 12.0f);
        _s.border_width = 1.0f;
        _s.border_color = KANA_TOOLBAR_BORDER_COLOR;
        rde_ui_image_set_style(_toolbar->palette, RDE_UI_STATE_NORMAL, _s);
        rde_ui_node_set_blocks_input(rde_ui_image_as_node(_toolbar->palette), true);
        rde_ui_node_add_child(_root, rde_ui_image_as_node(_toolbar->palette));

        for(u32 _i = 0; _i < KANA_TOOLBAR_PALETTE_COUNT; _i++) {
            _toolbar->swatch_refs[_i] = (kana_toolbar_swatch_ref){ _toolbar, _i };
            _toolbar->swatches[_i]    = rde_ui_button_create(NULL, NULL);
            kana_toolbar_button_colors(_toolbar->swatches[_i], KANA_TOOLBAR_PALETTE[_i], 2.0f, (rde_color){ 120, 120, 132, 255 });
            rde_ui_button_set_on_click(_toolbar->swatches[_i], kana_toolbar_on_swatch, &_toolbar->swatch_refs[_i]);
            rde_ui_node_add_child(rde_ui_image_as_node(_toolbar->palette), rde_ui_button_as_node(_toolbar->swatches[_i]));
        }
    }

    // Start on the right edge, vertically centred.
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    _toolbar->center = (rde_vec_2F){ _screen.x - 60.0f, _screen.y * 0.5f };

    kana_toolbar_layout(_toolbar);
    kana_toolbar_set_palette_open(_toolbar, false);
    kana_toolbar_refresh(_toolbar);
}

void kana_toolbar_destroy(kana_toolbar* _toolbar) {
    if(_toolbar->ui != NULL) {
        rde_ui_canvas_destroy(_toolbar->ui);
        _toolbar->ui = NULL;
    }

    if(_toolbar->font != NULL) {
        rde_font_unload(_toolbar->font);
        _toolbar->font = NULL;
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

    if(kana_toolbar_rect_contains(_toolbar->center, _toolbar->panel_size, _p)) {
        return true;
    }

    // Kept by kana_toolbar_place_palette, not read back from the UI: the computed
    // rect only updates at the next layout pass.
    if(_toolbar->palette_open && kana_toolbar_rect_contains(_toolbar->palette_center, _toolbar->palette_size, _p)) {
        return true;
    }

    return false;
}
