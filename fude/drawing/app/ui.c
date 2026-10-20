// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/ui.h"
#include "drawing/base/redraw.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "drawing/app/look.h"

// ===========================================================================
// See ui.h.
//
// Every widget lives on one RDE UI canvas. Building it (fude_ui_build) creates them all at once in their stacking
// order; a language change destroys the canvas and builds it again (fude_ui_rebuild), keeping only the state that is
// not shown from elsewhere: the toolbar's tool and place, and which of the side panel's parts are open. Each frame
// fude_ui_update shows the widgets that belong to the screen on top and hides the rest.
// ===========================================================================

// The icon fonts, in the app's assets (Roboto's files are draw.h's: FUDE_DRAW_FONT_REGULAR).
#define FUDE_UI_FONT_ICONS_PATH      "assets/fonts/Phosphor-Regular.ttf"
#define FUDE_UI_FONT_ICONS_FILL_PATH "assets/fonts/Phosphor-Fill.ttf"
#define FUDE_UI_FIELD                (rde_vec_2F){ 360.0f, 44.0f }   // A screen's field at the top right, at most.
// The field's font size.
#define FUDE_UI_FIELD_PX             14u

// Returns Phosphor Regular, or the UI font when Phosphor did not load.
rde_font* fude_ui_icon_font(const fude_ui* _ui) {
    return _ui->font_icons != NULL ? _ui->font_icons : _ui->font;
}

// --- a screen's field ----------------------------------------------------------------------

// Hands the field's text to its screen through _tell: on Return, or on each keystroke for a screen that searches as
// it is typed. Does nothing when the screen has no such callback.
RDE_INTERNAL void fude_ui_field_tell(const fude_ui_field_ref* _ref, void (*_tell)(any, const c8*)) {
    if(_tell == NULL) {
        return;
    }
    c8* _text = rde_ui_text_editor_get_text(_ref->field, 0, rde_ui_text_editor_get_byte_count(_ref->field));
    _tell(_ref->slot->self, _text != NULL ? _text : "");
    rde_ui_text_editor_free_text(_ref->field, _text);
}

// Return in a screen's field.
RDE_INTERNAL void fude_ui_on_field_submit(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_ui_field_ref* _ref = (const fude_ui_field_ref*)_user_data;
    fude_ui_field_tell(_ref, _ref->slot->vt->field_submit);
}

// A keystroke in a screen's field.
RDE_INTERNAL void fude_ui_on_field_change(rde_ui_node* _node, any _user_data) {
    RDE_UNUSED(_node);
    const fude_ui_field_ref* _ref = (const fude_ui_field_ref*)_user_data;
    fude_ui_field_tell(_ref, _ref->slot->vt->field_change);
}

// Builds the field that screen _slot declares (its hint, its most characters, its callbacks), hidden, with _ref as
// its callbacks' data. Returns the field.
RDE_INTERNAL rde_ui_text_editor* fude_ui_field_create(rde_ui_node* _root, fude_ui_field_ref* _ref, const fude_screen_slot* _slot) {
    rde_ui_text_editor* _field = rde_ui_text_editor_create(fude_kit_font(), NULL);
    _ref->field = _field;
    _ref->slot  = _slot;
    rde_ui_text_editor_set_multiline(_field, false);
    rde_ui_text_editor_set_font_size(_field, FUDE_UI_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_field, _slot->vt->field_max);
    rde_ui_text_editor_set_placeholder(_field, fude_text((FUDE_TEXT_)_slot->vt->field_hint));
    rde_ui_text_editor_set_content_insets(_field, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_text_editor_add_plugin(_field, rde_ui_text_editor_plugin_ime_get());   // Input methods' composition.
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_field), _ref);
    rde_ui_text_editor_set_on_submit(_field, fude_ui_on_field_submit);
    if(_slot->vt->field_change != NULL) {
        rde_ui_text_editor_set_on_change(_field, fude_ui_on_field_change);
    }
    fude_kit_field_box(_root, _field);
    rde_ui_node_set_active(fude_kit_field_node(_field), false);
    return _field;
}

// Writes where a screen's field goes, in UI canvas units: at the top right, on the line of its screen's title. On a
// phone (kit.h: compact) it takes a line of its own across the top, and the screen starts under it (fude_ui_frame).
RDE_INTERNAL void fude_ui_field_rect(const fude_ui* _ui, rde_vec_2F* _center, rde_vec_2F* _size) {
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // Left, top, right, bottom.
    const rde_vec_2F _most   = FUDE_UI_FIELD;
    if(fude_kit_compact(_ui->window)) {
        const f32 _w = _screen.x - (f32)_insets.x - (f32)_insets.z - 32.0f;
        *_size   = (rde_vec_2F){ _w, _most.y };
        *_center = (rde_vec_2F){ (f32)_insets.x + 16.0f + _w * 0.5f, _screen.y - (f32)_insets.y - 8.0f - _most.y * 0.5f };
        return;
    }
    const f32        _w      = rde_math_min_f32(_most.x, _screen.x - (f32)_insets.x - (f32)_insets.z - 180.0f);
    *_size   = (rde_vec_2F){ _w, _most.y };
    *_center = (rde_vec_2F){ _screen.x - (f32)_insets.z - 16.0f - _w * 0.5f, _screen.y - (f32)_insets.y - 8.0f - _most.y * 0.5f };
}

// Empties a field.
RDE_INTERNAL void fude_ui_field_clear(rde_ui_text_editor* _field) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
}

// --- each frame ------------------------------------------------------------------------------

// A row's `after` hook: brings the widgets up to date once a press is done.
void fude_ui_after_press(fude_app* _app) {
    fude_ui_update(_app->ui);
}

// Brings every widget up to date for the screen on top. See the header.
void fude_ui_update(fude_ui* _ui) {
    if(_ui->canvas == NULL) {
        return;
    }
    fude_app*               _app  = _ui->app;
    const fude_screen_slot* _top  = fude_app_top(_app);
    const b8                _full = _top != NULL || _ui->chrome_hidden;   // A screen is up, or the page alone shows.

    // When the safe area changes, the toolbar is clamped again: at launch iOS reports none (SDL has the whole window
    // until the view is laid out), and the real one arrives a frame or two later. The same happens when the screen's
    // size changes in UI units (the interface's size changed, or a computer's window was resized), and a bar at an
    // edge stays at that edge.
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    const b8         _sized  = rde_memcmp(&_screen, &_ui->_screen_seen, sizeof(rde_vec_2F)) != 0;
    if(rde_memcmp(&_insets, &_ui->_insets_seen, sizeof(rde_vec_4I)) != 0 || _sized) {
        if(_sized) {
            fude_toolbar_follow_screen(&_ui->bar, _ui->_screen_seen, _screen);
            _ui->_screen_seen = _screen;
        }
        _ui->_insets_seen = _insets;
        fude_toolbar_layout(&_ui->bar);
        fude_redraw_changed();   // (render on demand: a late safe area comes with no event)
    }

    fude_toolbar_update(&_ui->bar, _full);
    fude_pagemenu_update(&_ui->page, _full);
    fude_docbar_update(&_ui->docbar, _full || _ui->side.open);

    // Each screen's rows, bar and field: the top screen's are shown, the others hidden. A screen just opened starts
    // with its field empty. The fields are placed again when the screen's size or its top or right inset changes.
    rde_vec_2F _field_c, _field_s;
    fude_ui_field_rect(_ui, &_field_c, &_field_s);
    const rde_vec_4F _field_for = { _screen.x, _screen.y, (f32)_insets.y, (f32)_insets.z };
    const b8         _replace   = rde_memcmp(&_field_for, &_ui->_field_for, sizeof(rde_vec_4F)) != 0;
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
                rde_memset(_faces, 0, sizeof(_faces));
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
            // Place it as it shows, and again when the screen turns. A field is built the size of the whole window,
            // so shown unplaced it would cover everything.
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
        _ext->ui_update(_ui, _full);   // The app's own widgets (a study app's word card, for example).
    }
    fude_side_update(_ui, _full);
    fude_crashcard_update(_ui);
}

// Presses button _button of the top screen's row as a tap would, then updates the widgets. Returns false when there
// is no such button, or it is greyed out.
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

// Presses the top row's Back button, or else its Close, found by their words. Returns false when it has neither.
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

// Whether _screen (the app's screen space) is on any widget: the crash card, the side panel, the app's own, the page's
// menus, the toolbar, the document bar, a screen's field or bar, or a row.
b8 fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen) {
    if(_ui->canvas == NULL) {
        return false;
    }
    // From the app's screen space (centre origin) to UI canvas space (bottom-left origin).
    const rde_vec_2F _size = fude_kit_screen_size(_ui->window);
    const rde_vec_2F _p    = { _screen.x + _size.x * 0.5f, _screen.y + _size.y * 0.5f };

    // The crash card, the side panel and Settings: while any of them is open, it takes the whole screen.
    if(fude_crashcard_shown(_ui) || fude_side_hit(_ui, _p)) {
        return true;
    }
    const fude_extension* _ext = fude_app_ext(_ui->app);
    if((_ext->ui_hit != NULL && _ext->ui_hit(_ui, _screen, _p)) || fude_pagemenu_hit(&_ui->page, _p) || fude_toolbar_hit(&_ui->bar, _p) ||
       fude_docbar_hit(&_ui->docbar, _p)) {
        return true;
    }
    for(u32 _s = 0; _s < _ui->app->screen_count; _s++) {
        if(_ui->_field_shown[_s]) {
            // The field, and everything above it and to its right, up to the screen's edges.
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

// Fills in where screen _screen draws: from under its bar (or 8 units under the top of the safe area) down to 24 units
// above its row. See the header.
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
    if(!_bar && _ui->fields[_screen] != NULL && fude_kit_compact(_ui->window)) {
        _frame->top -= FUDE_UI_FIELD.y + 12.0f;   // A phone's field is on a line of its own above.
    }
    _frame->bottom  = -_hh + (f32)_safe.w + _frame->row_h + 24.0f;
}

// --- the theme ---------------------------------------------------------------------------

// Restyles every widget that was built from the active theme. Does nothing before the UI is built.
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
    fude_crashcard_restyle(_ui);
}

// --- lifetime -----------------------------------------------------------------------------

// Whether the UI is laid out right to left: a right-to-left language, or --rtl.
b8 fude_ui_rtl(void) {
    return rde_localization_is_rtl() || fude_look_rtl();
}

// Builds the canvas and every widget on it, in the language now (text.h), then styles, lays out and updates them.
RDE_INTERNAL void fude_ui_build(fude_ui* _ui) {
    fude_app* _app = _ui->app;
    _ui->_text_revision = fude_text_revision();
    // CONSTANT_PIXEL: one UI unit is one window unit, so the bar is the same size on every screen, and positions
    // match the pen's.
    _ui->canvas = rde_ui_canvas_create(_ui->window, RDE_UI_CANVAS_RENDER_MODE_SCREEN_OVERLAY, NULL);
    rde_ui_canvas_set_scale_mode(_ui->canvas, RDE_UI_CANVAS_SCALE_MODE_CONSTANT_PIXEL);
    // No automatic safe area. With it the canvas insets its ROOT, shifting every child up by the bottom inset (the
    // iPad's home-indicator strip), so the bar drew about 20 units above where fude_ui_hit tested it, and a pen
    // pressing the bar's top band wrote under it. The root must be exactly the window, the space pen positions are
    // in; fude_kit_clamp keeps what floats out of the unsafe edges instead.
    rde_ui_canvas_set_safe_area_enabled(_ui->canvas, false);
    // A right-to-left language (its .rdel block's @direction) mirrors the whole UI (rde.h: RDE_UI_DIRECTION_);
    // --rtl asks for it to check a layout.
    rde_ui_canvas_set_direction(_ui->canvas, fude_ui_rtl() ? RDE_UI_DIRECTION_RTL : RDE_UI_DIRECTION_LTR);
    fude_draw_set_rtl(fude_ui_rtl());
    rde_ui_node* _root = rde_ui_canvas_get_root(_ui->canvas);

    // Built in the order they stack, bottom first: the toolbar, the page's menus, the document bar, the screens' rows,
    // bars and fields; the side panel and Settings over them; the app's own widgets over those (a study app's word
    // card, for example, which opens from any screen); and the crash card over everything.
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
        // The rows are the screens' own tables, fixed in the code: too many is a programming error, caught here at
        // start rather than left for update and press to index past rows[].
        rde_assert_fatal(_rows + _slot->vt->row_count <= FUDE_UI_ROWS, "ui: the screens have more rows than FUDE_UI_ROWS (%u): raise it", FUDE_UI_ROWS);
        for(u32 _r = 0; _r < _slot->vt->row_count; _r++) {
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
        fude_app_ext(_app)->ui_build(_ui, _root);   // The app's own (a study app's word card, for example).
    }
    fude_crashcard_create(_ui, _root);   // Over everything: it asks about a crash report.

    _ui->_insets_seen = rde_window_get_safe_area_insets(_ui->window);
    _ui->_screen_seen = fude_kit_screen_size(_ui->window);
    fude_toolbar_layout(&_ui->bar);
    fude_ui_apply_theme(_ui);
    fude_ui_update(_ui);
}

// Sets the UI up for _app: the toolbar's starting tool, the fonts and their fallbacks, then every widget.
void fude_ui_init(fude_ui* _ui, fude_app* _app) {
    rde_memset(_ui, 0, sizeof(*_ui));
    _ui->app      = _app;
    _ui->window   = _app->window;
    _app->ui      = _ui;
    _ui->bar.tool        = FUDE_TOOL_DRAW;
    _ui->bar.tool_before = FUDE_TOOL_DRAW;
    _ui->bar.vertical    = true;

    // Every font is Slug with RDE's defaults. Its glyph textures start small and grow to what is drawn: more slots as
    // more glyphs show at once, and wider ones as bigger glyphs come (a kanji like 鬱 takes about 4 times a Latin
    // letter). So nothing is measured per font, and no glyph is left out for being too big. Going to the background
    // gives the memory back (fude_ui_trim_fonts).
    const rde_font_parameters _slug = RDE_DEFAULT_SLUG_FONT_PARAMETERS;
    _ui->font    = rde_font_load(FUDE_DRAW_FONT_REGULAR, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    // The language's script falls through to the app's font for it (for Japanese, Noto Sans JP), whose glyphs
    // load as they are first used.
    const c8* _script = _app->info->script_font;
    _ui->font_script  = _script != NULL ? rde_font_load(_script, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL) : NULL;
    if(_ui->font != NULL && _ui->font_script != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_script);
    }
    // Then the app's Latin font, for romanization's letters that Roboto lacks (Hindi's ṭ ḍ ṅ, Thai's tones ǎ and
    // vowels ɔ ʉ).
    const c8* _latin = _app->info->latin_font;
    _ui->font_latin  = _latin != NULL ? rde_font_load(_latin, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL) : NULL;
    if(_ui->font != NULL && _ui->font_latin != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_latin);
    }
    // The icons (icons.h): Phosphor, last in line, and Phosphor Fill as a font of its own.
    _ui->font_icons      = rde_font_load(FUDE_UI_FONT_ICONS_PATH, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    _ui->font_icons_fill = rde_font_load(FUDE_UI_FONT_ICONS_FILL_PATH, FUDE_KIT_FONT_SIZE, NULL, &_slug, NULL);
    if(_ui->font != NULL && _ui->font_icons != NULL) {
        rde_font_add_fallback(_ui->font, _ui->font_icons);
    }
    fude_kit_set_fonts(_ui->font, _ui->font_icons, _ui->font_icons_fill);
    fude_draw_set_icon_fill(_ui->font_icons_fill, (f32)FUDE_KIT_FONT_SIZE);   // The screens' filled icons (the marks).

    fude_pagemenu_init(&_ui->page, _app);
    fude_ui_build(_ui);
}

// The language changed: builds every widget again in it. What each shows comes back from the state it shows, and the
// side panel, Settings and the FAQ stay open if they were (Settings is where the language is chosen).
RDE_INTERNAL void fude_ui_rebuild(fude_ui* _ui) {
    const b8 _side_open     = _ui->side.open;
    const b8 _settings_open = _ui->side.settings_open;
    const b8 _faq_open      = _ui->side.faq_open;
    fude_side_forget(_ui);
    fude_crashcard_forget(_ui);
    if(fude_app_ext(_ui->app)->ui_forget != NULL) {
        fude_app_ext(_ui->app)->ui_forget(_ui);
    }
    rde_ui_canvas_destroy(_ui->canvas);
    _ui->canvas = NULL;

    // The widgets' own bookkeeping starts from nothing; only the toolbar's tool, its place and its folding stay.
    fude_toolbar _bar = _ui->bar;
    rde_memset(&_ui->bar, 0, sizeof(_ui->bar));
    _ui->bar.tool        = _bar.tool;
    _ui->bar.tool_before = _bar.tool_before;
    _ui->bar.vertical    = _bar.vertical;
    _ui->bar.center      = _bar.center;
    _ui->bar.minimized   = _bar.minimized;
    rde_memset(_ui->rows, 0, sizeof(_ui->rows));
    rde_memset(_ui->bars, 0, sizeof(_ui->bars));
    rde_memset(_ui->fields, 0, sizeof(_ui->fields));
    rde_memset(_ui->_field_shown, 0, sizeof(_ui->_field_shown));
    rde_memset(&_ui->_field_for, 0, sizeof(_ui->_field_for));
    fude_ui_build(_ui);
    _ui->side.open          = _side_open;
    _ui->side.settings_open = _settings_open;
    _ui->side.faq_open      = _faq_open;
}

// Builds every widget again now, when the UI has been built.
void fude_ui_rebuild_now(fude_ui* _ui) {
    if(_ui->canvas != NULL) {
        fude_ui_rebuild(_ui);
    }
}

// Rebuilds the UI when the language changed since it was built.
void fude_ui_follow_language(fude_ui* _ui) {
    if(_ui->canvas != NULL && _ui->_text_revision != fude_text_revision()) {
        fude_ui_rebuild(_ui);
    }
}

// Gives back the GPU memory every loaded font's glyphs grew into, and the memory of the texts' shapes kept for them
// (draw.h: fude_draw_forget).
void fude_ui_trim_fonts(fude_ui* _ui) {
    fude_draw_forget();
    rde_font* const _fonts[] = { _ui->font, _ui->font_script, _ui->font_latin, _ui->font_icons, _ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(_fonts[_i] != NULL) {
            rde_font_trim(_fonts[_i]);
        }
    }
}

// Lets go of the canvas and its widgets, then unloads the fonts, the UI font's fallbacks cleared first.
void fude_ui_destroy(fude_ui* _ui) {
    if(_ui->canvas != NULL) {
        fude_side_forget(_ui);
    fude_crashcard_forget(_ui);
        if(fude_app_ext(_ui->app)->ui_forget != NULL) {
            fude_app_ext(_ui->app)->ui_forget(_ui);
        }
        rde_ui_canvas_destroy(_ui->canvas);
        _ui->canvas = NULL;
    }
    // What draw.c keeps for the fonts' texts goes first: a font loaded later at a freed one's address would be taken
    // for it.
    fude_draw_forget();
    if(_ui->font != NULL) {
        rde_font_clear_fallbacks(_ui->font);
        rde_font_unload(_ui->font);
        _ui->font = NULL;
    }
    rde_font** const _fonts[] = { &_ui->font_script, &_ui->font_latin, &_ui->font_icons, &_ui->font_icons_fill };
    for(u32 _i = 0; _i < sizeof(_fonts) / sizeof(_fonts[0]); _i++) {
        if(*_fonts[_i] != NULL) {
            rde_font_unload(*_fonts[_i]);
            *_fonts[_i] = NULL;
        }
    }
}
