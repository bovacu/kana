#include "app/ui.h"
#include "widgets/kit.h"
#include "base/text.h"
#include "base/theme.h"
#include "widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See ui.h.
// ===========================================================================

#define KANA_UI_FONT_PATH            "assets/fonts/Roboto-Regular.ttf"
#define KANA_UI_FONT_JP_PATH         "assets/fonts/NotoSansJP-Regular.otf"
#define KANA_UI_FONT_ICONS_PATH      "assets/fonts/Phosphor-Regular.ttf"
#define KANA_UI_FONT_ICONS_FILL_PATH "assets/fonts/Phosphor-Fill.ttf"
#define KANA_UI_FIELD                (rde_vec_2F){ 360.0f, 44.0f }   // a screen's field at the top right, at most
#define KANA_UI_FIELD_PX             14u

rde_font* kana_ui_icon_font(const kana_ui* _ui) {
    return _ui->font_icons != NULL ? _ui->font_icons : _ui->font;
}

// --- a screen's field ----------------------------------------------------------------------

// Return in the field: its screen has what it says.
RDE_INTERNAL void kana_ui_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const kana_ui_field_ref* _ref = (const kana_ui_field_ref*)_user_data;
    c8* _text = rde_ui_text_editor_get_text(_ref->field, 0, rde_ui_text_editor_get_byte_count(_ref->field));
    _ref->slot->vt->field_submit(_ref->slot->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_ref->field, _text);
}

RDE_INTERNAL rde_ui_text_editor* kana_ui_field_create(rde_ui_node* _root, kana_ui_field_ref* _ref, const kana_screen_slot* _slot) {
    rde_ui_text_editor* _field = rde_ui_text_editor_create(kana_kit_font(), NULL);
    _ref->field = _field;
    _ref->slot  = _slot;
    rde_ui_text_editor_set_multiline(_field, false);
    rde_ui_text_editor_set_font_size(_field, KANA_UI_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_field, _slot->vt->field_max);
    rde_ui_text_editor_set_placeholder(_field, kana_text((KANA_TEXT_)_slot->vt->field_hint));
    rde_ui_text_editor_set_content_insets(_field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_text_editor_add_plugin(_field, rde_ui_text_editor_plugin_ime_get());   // the keyboard's composition, at the caret
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_field), _ref);
    rde_ui_text_editor_set_on_submit(_field, kana_ui_on_field_submit);
    kana_kit_field_box(_root, _field);
    rde_ui_node_set_active(kana_kit_field_node(_field), false);
    return _field;
}

// At the top right, on the row of its screen's title.
RDE_INTERNAL void kana_ui_field_rect(const kana_ui* _ui, rde_vec_2F* _center, rde_vec_2F* _size) {
    const rde_vec_2F _screen = kana_kit_screen_size(_ui->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
    const rde_vec_2F _most   = KANA_UI_FIELD;
    const f32        _w      = fminf(_most.x, _screen.x - (f32)_insets.x - (f32)_insets.z - 180.0f);
    *_size   = (rde_vec_2F){ _w, _most.y };
    *_center = (rde_vec_2F){ _screen.x - (f32)_insets.z - 16.0f - _w * 0.5f, _screen.y - (f32)_insets.y - 8.0f - _most.y * 0.5f };
}

RDE_INTERNAL void kana_ui_field_clear(rde_ui_text_editor* _field) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
}

// --- each frame ------------------------------------------------------------------------------

void kana_ui_after_press(kana_app* _app) {
    kana_ui_update(_app->ui);
}

void kana_ui_update(kana_ui* _ui) {
    if(_ui->canvas == NULL) {
        return;
    }
    kana_app*               _app  = _ui->app;
    const kana_screen_slot* _top  = kana_app_top(_app);
    const b8                _full = _top != NULL;   // a screen is up: the page's widgets make way

    // The safe area changed — at start iOS reports none (SDL has the whole
    // window until the view is laid out) and the real one arrives a frame or two
    // later: the bar is clamped again.
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    if(memcmp(&_insets, &_ui->_insets_seen, sizeof(rde_vec_4I)) != 0) {
        _ui->_insets_seen = _insets;
        kana_toolbar_layout(&_ui->bar);
    }

    kana_toolbar_update(&_ui->bar, _full);
    kana_pagemenu_update(&_ui->page, _full);

    // Each screen's rows, bar and field: the top one's shown, the others hidden.
    // A screen just opened starts with its field empty.
    rde_vec_2F _field_c, _field_s;
    kana_ui_field_rect(_ui, &_field_c, &_field_s);
    const rde_vec_2F _screen    = kana_kit_screen_size(_ui->window);
    const rde_vec_4F _field_for = { _screen.x, _screen.y, (f32)_insets.y, (f32)_insets.z };
    const b8         _replace   = memcmp(&_field_for, &_ui->_field_for, sizeof(rde_vec_4F)) != 0;
    _ui->_field_for = _field_for;
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        const kana_screen_slot* _slot = &_app->screens[_s];
        if(_slot->vt == NULL) {
            continue;
        }
        const b8  _open = _slot->vt->is_open(_slot->self);
        const b8  _up   = _slot == _top;
        const u32 _row  = _up && _slot->vt->row != NULL ? _slot->vt->row(_slot->self) : KANA_ROW_NONE;
        for(u32 _r = 0; _r < _slot->vt->row_count; _r++) {
            kana_row* _w = &_ui->rows[_ui->row_first[_s] + _r];
            if(_r == _row) {
                kana_row_face _faces[KANA_ROW_BUTTONS];
                memset(_faces, 0, sizeof(_faces));
                if(_slot->vt->faces != NULL) {
                    _slot->vt->faces(_slot->self, _r, _faces);
                }
                kana_row_apply(_w, _faces);
            }
            kana_row_show(_w, _ui->window, _r == _row, kana_row_bottom(_w, _ui->window));
        }
        if(_slot->vt->bar != NULL) {
            kana_filterbar_update(&_ui->bars[_s], _up);
        }
        rde_ui_text_editor* _field = _ui->fields[_s];
        if(_field != NULL) {
            if(_open && !_ui->_open_seen[_s]) {
                kana_ui_field_clear(_field);
            }
            if(_up != _ui->_field_shown[_s]) {
                _ui->_field_shown[_s] = _up;
                rde_ui_node_set_active(kana_kit_field_node(_field), _up);
            }
            if(_up && _replace) {
                kana_kit_place(kana_kit_field_node(_field), _field_c, _field_s);
            }
        }
        _ui->_open_seen[_s] = _open;
    }

    kana_wordcard_update(_ui);   // a word asked for (wordcard.h), opened; laid out again when the screen turns
    kana_side_update(_ui, _full);
}

b8 kana_ui_press(kana_ui* _ui, u32 _button) {
    const kana_screen_slot* _top = kana_app_top(_ui->app);
    if(_top == NULL || _top->vt->row == NULL) {
        return false;
    }
    const u32 _row = _top->vt->row(_top->self);
    if(_row >= _top->vt->row_count) {
        return false;
    }
    kana_row* _w = &_ui->rows[_ui->row_first[_top - _ui->app->screens] + _row];
    if(_button >= KANA_ROW_BUTTONS || _w->buttons[_button] == NULL || !rde_ui_button_as_node(_w->buttons[_button])->interactable) {
        return false;
    }
    const kana_row_button* _b = &_w->def->buttons[_button];
    _b->press(_ui->app, _w->self, _b->arg);
    kana_ui_update(_ui);
    return true;
}

b8 kana_ui_hit(const kana_ui* _ui, rde_vec_2F _screen) {
    if(_ui->canvas == NULL) {
        return false;
    }
    // Kana screen space (centre origin) → UI canvas space (bottom-left origin).
    const rde_vec_2F _size = kana_kit_screen_size(_ui->window);
    const rde_vec_2F _p    = { _screen.x + _size.x * 0.5f, _screen.y + _size.y * 0.5f };

    // The side panel and Settings (while either is open, the whole screen), and the word card.
    if(kana_side_hit(_ui, _p) || _ui->word.open) {
        return true;
    }
    if(kana_pagemenu_hit(&_ui->page, _screen, _p) || kana_toolbar_hit(&_ui->bar, _p)) {
        return true;
    }
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        if(_ui->_field_shown[_s]) {
            rde_vec_2F _c, _f;
            kana_ui_field_rect(_ui, &_c, &_f);
            if(_p.x >= _c.x - _f.x * 0.5f && _p.y >= _c.y - _f.y * 0.5f) {
                return true;
            }
        }
        if(_ui->app->screens[_s].vt != NULL && _ui->app->screens[_s].vt->bar != NULL && kana_filterbar_hit(&_ui->bars[_s], _p)) {
            return true;
        }
    }
    for(u32 _r = 0; _r < KANA_UI_ROWS; _r++) {
        if(kana_row_hit(&_ui->rows[_r], _p)) {
            return true;
        }
    }
    return false;
}

void kana_ui_frame(const kana_ui* _ui, KANA_SCREEN_ _screen, kana_screen_frame* _frame) {
    const kana_app*  _app  = _ui->app;
    const rde_vec_4I _safe = rde_window_get_safe_area_insets(_ui->window);
    const f32        _hh   = (f32)rde_window_get_size(_ui->window).y * 0.5f;
    const b8         _bar  = _app->screens[_screen].vt != NULL && _app->screens[_screen].vt->bar != NULL;
    _frame->window  = _ui->window;
    _frame->font    = _app->font;
    _frame->font_px = _app->font_px;
    _frame->row_h   = _app->screens[_screen].vt != NULL && _app->screens[_screen].vt->row_count > 0 ? kana_row_height() : 0.0f;
    _frame->top     = _bar ? _hh - _ui->bars[_screen].height : _hh - (f32)_safe.y - 8.0f;
    _frame->bottom  = -_hh + (f32)_safe.w + _frame->row_h + 24.0f;
}

// --- the theme ---------------------------------------------------------------------------

void kana_ui_apply_theme(kana_ui* _ui) {
    if(_ui->canvas == NULL) {
        return;
    }
    kana_toolbar_restyle(&_ui->bar);
    kana_pagemenu_restyle(&_ui->page);
    for(u32 _r = 0; _r < KANA_UI_ROWS; _r++) {
        if(_ui->rows[_r].panel != NULL) {
            kana_row_restyle(&_ui->rows[_r]);
        }
    }
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        kana_filterbar_restyle(&_ui->bars[_s]);
        if(_ui->fields[_s] != NULL) {
            kana_kit_style_field(_ui->fields[_s]);
        }
    }
    kana_wordcard_apply_theme(_ui);
    kana_side_apply_theme(_ui);
}

// --- lifetime -----------------------------------------------------------------------------

// Every widget, in the language now (text.h): the canvas and all on it.
RDE_INTERNAL void kana_ui_build(kana_ui* _ui) {
    kana_app* _app = _ui->app;
    _ui->_text_revision = kana_text_revision();
    // CONSTANT_PIXEL: one UI unit = one window unit, so the bar is the same size
    // on every screen, and positions match the pen's.
    _ui->canvas = rde_ui_canvas_create(_ui->window, RDE_UI_CANVAS_RENDER_MODE_SCREEN_OVERLAY, NULL);
    rde_ui_canvas_set_scale_mode(_ui->canvas, RDE_UI_CANVAS_SCALE_MODE_CONSTANT_PIXEL);
    // No automatic safe area. With it the canvas insets its ROOT — shifting every
    // child up by the bottom inset (the iPad's home-indicator strip) — so the bar
    // drew ~20 units above where kana_ui_hit tested it, and a pen pressing the
    // bar's top band wrote under it. The root must be exactly the window, the
    // space pen positions are in; kana_kit_clamp keeps what floats out of the
    // unsafe edges instead.
    rde_ui_canvas_set_safe_area_enabled(_ui->canvas, false);
    rde_ui_node* _root = rde_ui_canvas_get_root(_ui->canvas);

    // In the order they stack, bottom first: the bar, the page's menus, the
    // screens' rows, bars and fields; the side panel and Settings over them; the
    // word card over everything (it opens from any screen).
    kana_toolbar_create(&_ui->bar, _root, _app);
    kana_pagemenu_build(&_ui->page, _root);
    u32 _rows = 0;
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        const kana_screen_slot* _slot = &_app->screens[_s];
        _ui->row_first[_s] = _rows;
        if(_slot->vt == NULL) {
            continue;
        }
        for(u32 _r = 0; _r < _slot->vt->row_count && _rows < KANA_UI_ROWS; _r++) {
            kana_row_create(&_ui->rows[_rows++], _root, &_slot->vt->rows[_r], _app, _slot->self, kana_ui_after_press);
        }
        if(_slot->vt->bar != NULL) {
            kana_filterbar_create(&_ui->bars[_s], _root, _ui->window, _slot->vt->bar, _slot->self);
        }
        if(_slot->vt->field_hint != KANA_TEXT_COUNT && _slot->vt->field_submit != NULL) {
            _ui->fields[_s] = kana_ui_field_create(_root, &_ui->field_refs[_s], _slot);
        }
    }
    kana_side_create(_ui, _root);
    kana_wordcard_create(_ui, _root);

    _ui->_insets_seen = rde_window_get_safe_area_insets(_ui->window);
    kana_toolbar_layout(&_ui->bar);
    kana_ui_apply_theme(_ui);
    kana_ui_update(_ui);
}

void kana_ui_init(kana_ui* _ui, kana_app* _app) {
    memset(_ui, 0, sizeof(*_ui));
    _ui->app      = _app;
    _ui->window   = _app->window;
    _app->ui      = _ui;
    _ui->bar.tool        = KANA_TOOL_DRAW;
    _ui->bar.tool_before = KANA_TOOL_DRAW;
    _ui->bar.vertical    = true;

    // Every font is Slug with RDE's defaults: its glyph textures start small and
    // grow to what is drawn — more slots as more glyphs show at once, wider ones
    // as bigger glyphs come (a kanji like 鬱 takes ~4x a Latin letter) — so
    // nothing is measured per font, and no glyph is left out for being too big.
    // Going to the background gives the memory back (kana_ui_trim_fonts).
    const rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _ui->font    = rde_font_load(KANA_UI_FONT_PATH, KANA_KIT_FONT_SIZE, NULL, &_slug, NULL);
    // Japanese falls through to Noto Sans JP (its glyphs load as they are first used).
    _ui->font_jp = rde_font_load(KANA_UI_FONT_JP_PATH, KANA_KIT_FONT_SIZE, NULL, &_slug, NULL);
    if(_ui->font != NULL && _ui->font_jp != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_jp);
    }
    // The icons (icons.h): Phosphor, last in line, and a font of its own for Fill.
    _ui->font_icons      = rde_font_load(KANA_UI_FONT_ICONS_PATH, KANA_KIT_FONT_SIZE, NULL, &_slug, NULL);
    _ui->font_icons_fill = rde_font_load(KANA_UI_FONT_ICONS_FILL_PATH, KANA_KIT_FONT_SIZE, NULL, &_slug, NULL);
    if(_ui->font != NULL && _ui->font_icons != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_icons);
    }
    kana_kit_set_fonts(_ui->font, _ui->font_icons, _ui->font_icons_fill);
    kana_draw_set_icon_fill(_ui->font_icons_fill, (f32)KANA_KIT_FONT_SIZE);   // the screens' filled icons (the marks)

    kana_pagemenu_init(&_ui->page, _app);
    kana_ui_build(_ui);
}

// The language changed: every widget built again in it — what each shows comes
// back from the state it shows — and Settings still open if it was (it is where
// the language is chosen).
RDE_INTERNAL void kana_ui_rebuild(kana_ui* _ui) {
    const b8 _side_open     = _ui->side.open;
    const b8 _settings_open = _ui->side.settings_open;
    kana_side_forget(_ui);
    rde_ui_canvas_destroy(_ui->canvas);
    _ui->canvas = NULL;

    // The widgets' own bookkeeping, from nothing; the toolbar's tool and place stay.
    kana_toolbar _bar = _ui->bar;
    memset(&_ui->bar, 0, sizeof(_ui->bar));
    _ui->bar.tool        = _bar.tool;
    _ui->bar.tool_before = _bar.tool_before;
    _ui->bar.vertical    = _bar.vertical;
    _ui->bar.center      = _bar.center;
    _ui->bar.minimized   = _bar.minimized;
    memset(_ui->rows, 0, sizeof(_ui->rows));
    memset(_ui->bars, 0, sizeof(_ui->bars));
    memset(_ui->fields, 0, sizeof(_ui->fields));
    memset(_ui->_field_shown, 0, sizeof(_ui->_field_shown));
    memset(&_ui->_field_for, 0, sizeof(_ui->_field_for));
    kana_ui_build(_ui);
    _ui->side.open          = _side_open;
    _ui->side.settings_open = _settings_open;
}

void kana_ui_follow_language(kana_ui* _ui) {
    if(_ui->canvas != NULL && _ui->_text_revision != kana_text_revision()) {
        kana_ui_rebuild(_ui);
    }
}

void kana_ui_trim_fonts(kana_ui* _ui) {
    rde_font* const _fonts[] = { _ui->font, _ui->font_jp, _ui->font_icons, _ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(_fonts[_i] != NULL) {
            rde_font_trim(_fonts[_i]);
        }
    }
}

void kana_ui_destroy(kana_ui* _ui) {
    if(_ui->canvas != NULL) {
        kana_side_forget(_ui);
        rde_ui_canvas_destroy(_ui->canvas);
        _ui->canvas = NULL;
    }
    kana_pagemenu_destroy(&_ui->page);
    if(_ui->font != NULL) {
        rde_font_clear_fallbacks(_ui->font);
        rde_font_unload(_ui->font);
        _ui->font = NULL;
    }
    rde_font** const _fonts[] = { &_ui->font_jp, &_ui->font_icons, &_ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(*_fonts[_i] != NULL) {
            rde_font_unload(*_fonts[_i]);
            *_fonts[_i] = NULL;
        }
    }
}
