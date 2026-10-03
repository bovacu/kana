#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See ui.h.
// ===========================================================================

#define FUDE_UI_FONT_PATH            "assets/fonts/Roboto-Regular.ttf"
#define FUDE_UI_FONT_ICONS_PATH      "assets/fonts/Phosphor-Regular.ttf"
#define FUDE_UI_FONT_ICONS_FILL_PATH "assets/fonts/Phosphor-Fill.ttf"
#define FUDE_UI_FIELD                (rde_vec_2F){ 360.0f, 44.0f }   // a screen's field at the top right, at most
#define FUDE_UI_FIELD_PX             14u

rde_font* fude_ui_icon_font(const fude_ui* _ui) {
    return _ui->font_icons != NULL ? _ui->font_icons : _ui->font;
}

// --- a screen's field ----------------------------------------------------------------------

// Return in the field, or (a screen that searches as it is typed) a keystroke:
// its screen has what it says.
RDE_INTERNAL void fude_ui_field_tell(const fude_ui_field_ref* _ref, void (*_tell)(void*, const c8*)) {
    if(_tell == NULL) {
        return;
    }
    c8* _text = rde_ui_text_editor_get_text(_ref->field, 0, rde_ui_text_editor_get_byte_count(_ref->field));
    _tell(_ref->slot->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_ref->field, _text);
}

RDE_INTERNAL void fude_ui_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_ui_field_ref* _ref = (const fude_ui_field_ref*)_user_data;
    fude_ui_field_tell(_ref, _ref->slot->vt->field_submit);
}

RDE_INTERNAL void fude_ui_on_field_change(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_ui_field_ref* _ref = (const fude_ui_field_ref*)_user_data;
    fude_ui_field_tell(_ref, _ref->slot->vt->field_change);
}

RDE_INTERNAL rde_ui_text_editor* fude_ui_field_create(rde_ui_node* _root, fude_ui_field_ref* _ref, const fude_screen_slot* _slot) {
    rde_ui_text_editor* _field = rde_ui_text_editor_create(fude_kit_font(), NULL);
    _ref->field = _field;
    _ref->slot  = _slot;
    rde_ui_text_editor_set_multiline(_field, false);
    rde_ui_text_editor_set_font_size(_field, FUDE_UI_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_field, _slot->vt->field_max);
    rde_ui_text_editor_set_placeholder(_field, fude_text((FUDE_TEXT_)_slot->vt->field_hint));
    rde_ui_text_editor_set_content_insets(_field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_text_editor_add_plugin(_field, rde_ui_text_editor_plugin_ime_get());   // the keyboard's composition, at the caret
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_field), _ref);
    rde_ui_text_editor_set_on_submit(_field, fude_ui_on_field_submit);
    if(_slot->vt->field_change != NULL) {
        rde_ui_text_editor_set_on_change(_field, fude_ui_on_field_change);
    }
    fude_kit_field_box(_root, _field);
    rde_ui_node_set_active(fude_kit_field_node(_field), false);
    return _field;
}

// At the top right, on the row of its screen's title.
RDE_INTERNAL void fude_ui_field_rect(const fude_ui* _ui, rde_vec_2F* _center, rde_vec_2F* _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
    const rde_vec_2F _most   = FUDE_UI_FIELD;
    const f32        _w      = fminf(_most.x, _screen.x - (f32)_insets.x - (f32)_insets.z - 180.0f);
    *_size   = (rde_vec_2F){ _w, _most.y };
    *_center = (rde_vec_2F){ _screen.x - (f32)_insets.z - 16.0f - _w * 0.5f, _screen.y - (f32)_insets.y - 8.0f - _most.y * 0.5f };
}

RDE_INTERNAL void fude_ui_field_clear(rde_ui_text_editor* _field) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
}

// --- each frame ------------------------------------------------------------------------------

void fude_ui_after_press(fude_app* _app) {
    fude_ui_update(_app->ui);
}

void fude_ui_update(fude_ui* _ui) {
    if(_ui->canvas == NULL) {
        return;
    }
    fude_app*               _app  = _ui->app;
    const fude_screen_slot* _top  = fude_app_top(_app);
    const b8                _full = _top != NULL;   // a screen is up: the page's widgets make way

    // The safe area changed — at start iOS reports none (SDL has the whole
    // window until the view is laid out) and the real one arrives a frame or two
    // later: the bar is clamped again.
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    if(memcmp(&_insets, &_ui->_insets_seen, sizeof(rde_vec_4I)) != 0) {
        _ui->_insets_seen = _insets;
        fude_toolbar_layout(&_ui->bar);
    }

    fude_toolbar_update(&_ui->bar, _full);
    fude_pagemenu_update(&_ui->page, _full);
    fude_docbar_update(&_ui->docbar, _full || _ui->side.open);

    // Each screen's rows, bar and field: the top one's shown, the others hidden.
    // A screen just opened starts with its field empty.
    rde_vec_2F _field_c, _field_s;
    fude_ui_field_rect(_ui, &_field_c, &_field_s);
    const rde_vec_2F _screen    = fude_kit_screen_size(_ui->window);
    const rde_vec_4F _field_for = { _screen.x, _screen.y, (f32)_insets.y, (f32)_insets.z };
    const b8         _replace   = memcmp(&_field_for, &_ui->_field_for, sizeof(rde_vec_4F)) != 0;
    _ui->_field_for = _field_for;
    for(u32 _s = 0; _s < _ui->app->screen_count; _s++) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        if(_slot->vt == NULL) {
            continue;
        }
        const b8  _open = _slot->vt->is_open(_slot->self);
        const b8  _up   = _slot == _top;
        const u32 _row  = _up && _slot->vt->row != NULL ? _slot->vt->row(_slot->self) : FUDE_ROW_NONE;
        for(u32 _r = 0; _r < _slot->vt->row_count; _r++) {
            fude_row* _w = &_ui->rows[_ui->row_first[_s] + _r];
            if(_r == _row) {
                fude_row_face _faces[FUDE_ROW_BUTTONS];
                memset(_faces, 0, sizeof(_faces));
                if(_slot->vt->faces != NULL) {
                    _slot->vt->faces(_slot->self, _r, _faces);
                }
                fude_row_apply(_w, _faces);
            }
            fude_row_show(_w, _ui->window, _r == _row, fude_row_bottom(_w, _ui->window));
        }
        if(_slot->vt->bar != NULL) {
            fude_filterbar_update(&_ui->bars[_s], _up);
        }
        rde_ui_text_editor* _field = _ui->fields[_s];
        if(_field != NULL) {
            if(_open && !_ui->_open_seen[_s]) {
                fude_ui_field_clear(_field);
            }
            // Placed as it shows, and again when the screen turns (built, it is the
            // whole window: shown unplaced, it would cover everything).
            const b8 _shows = _up && !_ui->_field_shown[_s];
            if(_up != _ui->_field_shown[_s]) {
                _ui->_field_shown[_s] = _up;
                rde_ui_node_set_active(fude_kit_field_node(_field), _up);
            }
            if(_up && (_replace || _shows)) {
                fude_kit_place(fude_kit_field_node(_field), _field_c, _field_s);
            }
        }
        _ui->_open_seen[_s] = _open;
    }

    const fude_extension* _ext = fude_app_ext(_app);
    if(_ext->ui_update != NULL) {
        _ext->ui_update(_ui, _full);   // the app's own widgets (Kana: the word card)
    }
    fude_side_update(_ui, _full);
}

b8 fude_ui_press(fude_ui* _ui, u32 _button) {
    const fude_screen_slot* _top = fude_app_top(_ui->app);
    if(_top == NULL || _top->vt->row == NULL) {
        return false;
    }
    const u32 _row = _top->vt->row(_top->self);
    if(_row >= _top->vt->row_count) {
        return false;
    }
    fude_row* _w = &_ui->rows[_ui->row_first[_top - _ui->app->screens] + _row];
    if(_button >= FUDE_ROW_BUTTONS || _w->buttons[_button] == NULL || !rde_ui_button_as_node(_w->buttons[_button])->interactable) {
        return false;
    }
    const fude_row_button* _b = &_w->def->buttons[_button];
    _b->press(_ui->app, _w->self, _b->arg);
    fude_ui_update(_ui);
    return true;
}

b8 fude_ui_press_back(fude_ui* _ui) {
    const fude_screen_slot* _top = fude_app_top(_ui->app);
    if(_top == NULL || _top->vt->row == NULL) {
        return false;
    }
    const u32 _row = _top->vt->row(_top->self);
    if(_row >= _top->vt->row_count) {
        return false;
    }
    const fude_row* _w = &_ui->rows[_ui->row_first[_top - _ui->app->screens] + _row];
    static const u32 FUDE_UI_BACK_TEXTS[] = { FUDE_TEXT_BACK, FUDE_TEXT_CLOSE };
    for(u32 _t = 0; _t < sizeof(FUDE_UI_BACK_TEXTS) / sizeof(FUDE_UI_BACK_TEXTS[0]); _t++) {
        for(u32 _b = 0; _b < _w->def->count && _b < FUDE_ROW_BUTTONS; _b++) {
            if(_w->def->buttons[_b].text == FUDE_UI_BACK_TEXTS[_t] && _w->buttons[_b] != NULL) {
                return fude_ui_press(_ui, _b);
            }
        }
    }
    return false;
}

b8 fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen) {
    if(_ui->canvas == NULL) {
        return false;
    }
    // The app's screen space (centre origin) → UI canvas space (bottom-left origin).
    const rde_vec_2F _size = fude_kit_screen_size(_ui->window);
    const rde_vec_2F _p    = { _screen.x + _size.x * 0.5f, _screen.y + _size.y * 0.5f };

    // The side panel and Settings (while either is open, the whole screen).
    if(fude_side_hit(_ui, _p)) {
        return true;
    }
    const fude_extension* _ext = fude_app_ext(_ui->app);
    if((_ext->ui_hit != NULL && _ext->ui_hit(_ui, _screen, _p)) || fude_pagemenu_hit(&_ui->page, _p) || fude_toolbar_hit(&_ui->bar, _p) ||
       fude_docbar_hit(&_ui->docbar, _p)) {
        return true;
    }
    for(u32 _s = 0; _s < _ui->app->screen_count; _s++) {
        if(_ui->_field_shown[_s]) {
            rde_vec_2F _c, _f;
            fude_ui_field_rect(_ui, &_c, &_f);
            if(_p.x >= _c.x - _f.x * 0.5f && _p.y >= _c.y - _f.y * 0.5f) {
                return true;
            }
        }
        if(_ui->app->screens[_s].vt != NULL && _ui->app->screens[_s].vt->bar != NULL && fude_filterbar_hit(&_ui->bars[_s], _p)) {
            return true;
        }
    }
    for(u32 _r = 0; _r < FUDE_UI_ROWS; _r++) {
        if(fude_row_hit(&_ui->rows[_r], _p)) {
            return true;
        }
    }
    return false;
}

void fude_ui_frame(const fude_ui* _ui, u32 _screen, fude_screen_frame* _frame) {
    const fude_app*  _app  = _ui->app;
    const rde_vec_4I _safe = rde_window_get_safe_area_insets(_ui->window);
    const f32        _hh   = (f32)rde_window_get_size(_ui->window).y * 0.5f;
    const b8         _bar  = _app->screens[_screen].vt != NULL && _app->screens[_screen].vt->bar != NULL;
    _frame->window  = _ui->window;
    _frame->font    = _app->font;
    _frame->font_px = _app->font_px;
    _frame->row_h   = _app->screens[_screen].vt != NULL && _app->screens[_screen].vt->row_count > 0 ? fude_row_height() : 0.0f;
    _frame->top     = _bar ? _hh - _ui->bars[_screen].height : _hh - (f32)_safe.y - 8.0f;
    _frame->bottom  = -_hh + (f32)_safe.w + _frame->row_h + 24.0f;
}

// --- the theme ---------------------------------------------------------------------------

void fude_ui_apply_theme(fude_ui* _ui) {
    if(_ui->canvas == NULL) {
        return;
    }
    fude_toolbar_restyle(&_ui->bar);
    fude_pagemenu_restyle(&_ui->page);
    fude_docbar_restyle(&_ui->docbar);
    for(u32 _r = 0; _r < FUDE_UI_ROWS; _r++) {
        if(_ui->rows[_r].panel != NULL) {
            fude_row_restyle(&_ui->rows[_r]);
        }
    }
    for(u32 _s = 0; _s < _ui->app->screen_count; _s++) {
        fude_filterbar_restyle(&_ui->bars[_s]);
        if(_ui->fields[_s] != NULL) {
            fude_kit_style_field(_ui->fields[_s]);
        }
    }
    if(fude_app_ext(_ui->app)->ui_restyle != NULL) {
        fude_app_ext(_ui->app)->ui_restyle(_ui);
    }
    fude_side_apply_theme(_ui);
}

// --- lifetime -----------------------------------------------------------------------------

// Every widget, in the language now (text.h): the canvas and all on it.
RDE_INTERNAL void fude_ui_build(fude_ui* _ui) {
    fude_app* _app = _ui->app;
    _ui->_text_revision = fude_text_revision();
    // CONSTANT_PIXEL: one UI unit = one window unit, so the bar is the same size
    // on every screen, and positions match the pen's.
    _ui->canvas = rde_ui_canvas_create(_ui->window, RDE_UI_CANVAS_RENDER_MODE_SCREEN_OVERLAY, NULL);
    rde_ui_canvas_set_scale_mode(_ui->canvas, RDE_UI_CANVAS_SCALE_MODE_CONSTANT_PIXEL);
    // No automatic safe area. With it the canvas insets its ROOT — shifting every
    // child up by the bottom inset (the iPad's home-indicator strip) — so the bar
    // drew ~20 units above where fude_ui_hit tested it, and a pen pressing the
    // bar's top band wrote under it. The root must be exactly the window, the
    // space pen positions are in; fude_kit_clamp keeps what floats out of the
    // unsafe edges instead.
    rde_ui_canvas_set_safe_area_enabled(_ui->canvas, false);
    rde_ui_node* _root = rde_ui_canvas_get_root(_ui->canvas);

    // In the order they stack, bottom first: the bar, the page's menus, the
    // screens' rows, bars and fields; the side panel and Settings over them; the
    // app's own over everything (Kana's word card: it opens from any screen).
    fude_toolbar_create(&_ui->bar, _root, _app);
    fude_pagemenu_build(&_ui->page, _root);
    fude_docbar_create(&_ui->docbar, _root, _app);
    u32 _rows = 0;
    for(u32 _s = 0; _s < _ui->app->screen_count; _s++) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        _ui->row_first[_s] = _rows;
        if(_slot->vt == NULL) {
            continue;
        }
        for(u32 _r = 0; _r < _slot->vt->row_count && _rows < FUDE_UI_ROWS; _r++) {
            fude_row_create(&_ui->rows[_rows++], _root, &_slot->vt->rows[_r], _app, _slot->self, fude_ui_after_press);
        }
        if(_slot->vt->bar != NULL) {
            fude_filterbar_create(&_ui->bars[_s], _root, _ui->window, _slot->vt->bar, _slot->self);
        }
        if(_slot->vt->field_hint != FUDE_TEXT_COUNT && (_slot->vt->field_submit != NULL || _slot->vt->field_change != NULL)) {
            _ui->fields[_s] = fude_ui_field_create(_root, &_ui->field_refs[_s], _slot);
        }
    }
    fude_side_create(_ui, _root);
    if(fude_app_ext(_app)->ui_build != NULL) {
        fude_app_ext(_app)->ui_build(_ui, _root);   // the app's own, over all (Kana: the word card)
    }

    _ui->_insets_seen = rde_window_get_safe_area_insets(_ui->window);
    fude_toolbar_layout(&_ui->bar);
    fude_ui_apply_theme(_ui);
    fude_ui_update(_ui);
}

void fude_ui_init(fude_ui* _ui, fude_app* _app) {
    memset(_ui, 0, sizeof(*_ui));
    _ui->app      = _app;
    _ui->window   = _app->window;
    _app->ui      = _ui;
    _ui->bar.tool        = FUDE_TOOL_DRAW;
    _ui->bar.tool_before = FUDE_TOOL_DRAW;
    _ui->bar.vertical    = true;

    // Every font is Slug with RDE's defaults: its glyph textures start small and
    // grow to what is drawn — more slots as more glyphs show at once, wider ones
    // as bigger glyphs come (a kanji like 鬱 takes ~4x a Latin letter) — so
    // nothing is measured per font, and no glyph is left out for being too big.
    // Going to the background gives the memory back (fude_ui_trim_fonts).
    const rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _ui->font    = rde_font_load(FUDE_UI_FONT_PATH, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    // The language's script falls through to the app's font for it (Kana's
    // Japanese: Noto Sans JP; its glyphs load as they are first used).
    const c8* _script = _app->info != NULL ? _app->info->script_font : NULL;
    _ui->font_script  = _script != NULL ? rde_font_load(_script, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL) : NULL;
    if(_ui->font != NULL && _ui->font_script != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_script);
    }
    // The icons (icons.h): Phosphor, last in line, and a font of its own for Fill.
    _ui->font_icons      = rde_font_load(FUDE_UI_FONT_ICONS_PATH, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    _ui->font_icons_fill = rde_font_load(FUDE_UI_FONT_ICONS_FILL_PATH, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    if(_ui->font != NULL && _ui->font_icons != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_icons);
    }
    fude_kit_set_fonts(_ui->font, _ui->font_icons, _ui->font_icons_fill);
    fude_draw_set_icon_fill(_ui->font_icons_fill, (f32)FUDE_KIT_FONT_SIZE);   // the screens' filled icons (the marks)

    fude_pagemenu_init(&_ui->page, _app);
    fude_ui_build(_ui);
}

// The language changed: every widget built again in it — what each shows comes
// back from the state it shows — and Settings still open if it was (it is where
// the language is chosen).
RDE_INTERNAL void fude_ui_rebuild(fude_ui* _ui) {
    const b8 _side_open     = _ui->side.open;
    const b8 _settings_open = _ui->side.settings_open;
    fude_side_forget(_ui);
    if(fude_app_ext(_ui->app)->ui_forget != NULL) {
        fude_app_ext(_ui->app)->ui_forget(_ui);
    }
    rde_ui_canvas_destroy(_ui->canvas);
    _ui->canvas = NULL;

    // The widgets' own bookkeeping, from nothing; the toolbar's tool and place stay.
    fude_toolbar _bar = _ui->bar;
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
    fude_ui_build(_ui);
    _ui->side.open          = _side_open;
    _ui->side.settings_open = _settings_open;
}

void fude_ui_follow_language(fude_ui* _ui) {
    if(_ui->canvas != NULL && _ui->_text_revision != fude_text_revision()) {
        fude_ui_rebuild(_ui);
    }
}

void fude_ui_trim_fonts(fude_ui* _ui) {
    rde_font* const _fonts[] = { _ui->font, _ui->font_script, _ui->font_icons, _ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(_fonts[_i] != NULL) {
            rde_font_trim(_fonts[_i]);
        }
    }
}

void fude_ui_destroy(fude_ui* _ui) {
    if(_ui->canvas != NULL) {
        fude_side_forget(_ui);
        if(fude_app_ext(_ui->app)->ui_forget != NULL) {
            fude_app_ext(_ui->app)->ui_forget(_ui);
        }
        rde_ui_canvas_destroy(_ui->canvas);
        _ui->canvas = NULL;
    }
    if(_ui->font != NULL) {
        rde_font_clear_fallbacks(_ui->font);
        rde_font_unload(_ui->font);
        _ui->font = NULL;
    }
    rde_font** const _fonts[] = { &_ui->font_script, &_ui->font_icons, &_ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(*_fonts[_i] != NULL) {
            rde_font_unload(*_fonts[_i]);
            *_fonts[_i] = NULL;
        }
    }
}
