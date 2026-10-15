// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/settings.h"
#include "drawing/base/kfile.h"
#include "drawing/base/theme.h"
#include "drawing/ink/canvas.h"

// ===========================================================================
// See settings.h, which lists the fields of the 'PREF' chunk in their order.
//
// Reading follows the same order field by field. A file from an older build simply ends early: every field after
// its end fails to read and keeps the value the settings came in with, so the writer and the reader must only ever grow
// at the end. A few fields get their own default for old files instead (see fude_settings_read).
// ===========================================================================

// The file's kind ('SETT') and its one chunk ('PREF'), as KANA tags (kfile.h).
#define FUDE_KIND_SETTINGS  FUDE_TAG('S', 'E', 'T', 'T')
#define FUDE_CHUNK_PREFS    FUDE_TAG('P', 'R', 'E', 'F')

// Writes the header and the 'PREF' chunk, every field in the order settings.h lists, then writes the file atomically.
// Returns false when it could not be written.
b8 fude_save_settings(const c8* _path, const fude_settings* _settings) {
    fude_bytes _b = fude_bytes_new(64u);
    fude_put_header(&_b, FUDE_SETTINGS_VERSION, FUDE_KIND_SETTINGS);

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_PREFS);
    fude_put_u8(&_b, _settings->tool);
    fude_put_u8(&_b, _settings->vertical ? 1u : 0u);
    fude_put_u8(&_b, _settings->show_hud ? 1u : 0u);
    fude_put_u8(&_b, _settings->brush_scale);
    fude_put_u8(&_b, _settings->width_mode);
    fude_put_color(&_b, _settings->color);
    fude_put_f32(&_b, _settings->radius);
    fude_put_f32(&_b, _settings->toolbar_center.x);
    fude_put_f32(&_b, _settings->toolbar_center.y);
    fude_put_u8(&_b, _settings->theme);
    fude_put_u8(&_b, _settings->mlkit ? 1u : 0u);
    fude_put_u8(&_b, _settings->toolbar_minimized ? 1u : 0u);
    fude_put_u8(&_b, _settings->paper_size);
    fude_put_u8(&_b, _settings->language);
    fude_put_u8(&_b, _settings->finger_writes ? 1u : 0u);
    fude_put_u8(&_b, _settings->pen_ever ? 1u : 0u);
    fude_put_color(&_b, _settings->marker_color);
    fude_put_f32(&_b, _settings->marker_radius);
    fude_put_u8(&_b, _settings->cards_read);
    fude_put_u8(&_b, _settings->ui_size);
    fude_put_u8(&_b, _settings->smoothing);
    fude_put_f32(&_b, _settings->eraser_radius);
    fude_put_u8(&_b, _settings->units);
    fude_put_f32(&_b, _settings->true_mm_per_point);
    fude_put_f32(&_b, _settings->print_x);
    fude_put_f32(&_b, _settings->print_y);
    fude_put_f32(&_b, _settings->kerf_mm);
    fude_put_u8(&_b, _settings->show_fps ? 1u : 0u);
    fude_put_u8(&_b, _settings->keep_grain ? 1u : 0u);
    fude_put_u8(&_b, _settings->smooth_chosen ? 1u : 0u);
    fude_put_u8(&_b, 1u);   // mlkit_chosen: the mlkit byte is a person's choice (settings.h)
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// Reads a settings file's bytes (_data, _size) over the fude_settings that _user points to; fude_save_load (save.h)
// calls it with the file or its backup. The fields are read into a copy that is committed only at the end, so a file
// whose header does not read leaves the settings as they came. Unknown chunks are skipped. Returns false when the
// header is not a settings file's.
RDE_INTERNAL b8 fude_settings_read(const u8* _data, u32 _size, any _user) {
    fude_settings* _settings = (fude_settings*)_user;
    fude_reader    _r        = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_SETTINGS_VERSION, FUDE_KIND_SETTINGS)) {
        return false;
    }

    fude_settings _s = *_settings;
    _s.smooth_chosen = false;   // Older files stored a smoothing that was never chosen by hand, so the default applies.

    while(_r.ok && _r.pos < _r.size) {
        const u32 _tag = fude_get_u32(&_r);
        const u32 _len = fude_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            break;
        }

        fude_reader _c  = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos         += _len;

        if(_tag != FUDE_CHUNK_PREFS) {
            continue;
        }

        // Read field by field, in order. A file from an older build simply ends early, and each field it has is taken
        // only if it is in range; an out-of-range value keeps the value the settings came in with.
        const u8 _tool = fude_get_u8(&_c);
        if(_c.ok && _tool <= 3u) { _s.tool = _tool; }   // FUDE_TOOL_: draw, erase, lasso or mark.
        const u8 _vertical = fude_get_u8(&_c);
        if(_c.ok) { _s.vertical = _vertical != 0; }
        const u8 _hud = fude_get_u8(&_c);
        if(_c.ok) { _s.show_hud = _hud != 0; }
        const u8 _scale = fude_get_u8(&_c);
        if(_c.ok && _scale <= 1u) { _s.brush_scale = _scale; }
        const u8 _width = fude_get_u8(&_c);
        if(_c.ok && _width <= 1u) { _s.width_mode = _width; }
        const rde_color _color = fude_get_color(&_c);
        if(_c.ok) { _s.color = fude_theme_saved_color(_color); }
        const f32 _radius = fude_get_f32(&_c);
        if(_c.ok && rde_math_is_finite_f32(_radius) && _radius > 0.0f) { _s.radius = _radius; }
        const f32 _cx = fude_get_f32(&_c);
        const f32 _cy = fude_get_f32(&_c);
        if(_c.ok && rde_math_is_finite_f32(_cx) && rde_math_is_finite_f32(_cy)) { _s.toolbar_center = (rde_vec_2F){ _cx, _cy }; }
        const u8 _theme = fude_get_u8(&_c);
        if(_c.ok && _theme < FUDE_THEME_COUNT) { _s.theme = _theme; }
        const u8 _mlkit = fude_get_u8(&_c);   // (taken at the end, only if mlkit_chosen says it is a choice)
        const b8 _mlkit_read = _c.ok && _mlkit <= 1u;
        const u8 _minimized = fude_get_u8(&_c);
        if(_c.ok && _minimized <= 1u) { _s.toolbar_minimized = _minimized != 0; }
        const u8 _paper = fude_get_u8(&_c);
        if(_c.ok && _paper < FUDE_PAPER_SIZE_COUNT) { _s.paper_size = _paper; }
        const u8 _language = fude_get_u8(&_c);
        if(_c.ok && _language < RDE_LANGUAGE_COUNT) { _s.language = _language; }
        // A file from before the toolbar's hand belonged to a pen user, so the pen writes and fingers move the page.
        _s.finger_writes = false;
        _s.pen_ever      = true;
        const u8 _finger = fude_get_u8(&_c);
        if(_c.ok && _finger <= 1u) { _s.finger_writes = _finger != 0; }
        const u8 _pen = fude_get_u8(&_c);
        if(_c.ok && _pen <= 1u) { _s.pen_ever = _pen != 0; }
        const rde_color _marker = fude_get_color(&_c);
        if(_c.ok && _marker.a > 0u) { _s.marker_color = _marker; }
        const f32 _marker_radius = fude_get_f32(&_c);
        if(_c.ok && rde_math_is_finite_f32(_marker_radius) && _marker_radius > 0.0f) { _s.marker_radius = _marker_radius; }
        const u8 _cards = fude_get_u8(&_c);   // Older files have none read.
        if(_c.ok) { _s.cards_read = _cards; }
        const u8 _ui_size = fude_get_u8(&_c);   // Older files use the device's size (0).
        if(_c.ok && _ui_size < 4u) { _s.ui_size = _ui_size; }
        const u8 _smoothing = fude_get_u8(&_c);   // Older files: never chosen (0).
        if(_c.ok && _smoothing < 16u) { _s.smoothing = _smoothing; }
        const f32 _eraser = fude_get_f32(&_c);   // Older files: never chosen (0).
        if(_c.ok && rde_math_is_finite_f32(_eraser) && _eraser > 0.0f) { _s.eraser_radius = _eraser; }
        const u8 _units = fude_get_u8(&_c);   // Older files have no units; millimetres.
        if(_c.ok && _units < FUDE_SETTINGS_UNITS) { _s.units = _units; }
        const f32 _true = fude_get_f32(&_c);   // Older files: not calibrated.
        if(_c.ok && rde_math_is_finite_f32(_true) && _true > 0.0f && _true < 10.0f) { _s.true_mm_per_point = _true; }
        const f32 _print_x = fude_get_f32(&_c), _print_y = fude_get_f32(&_c);   // Older files: printer never checked.
        if(_c.ok && rde_math_is_finite_f32(_print_x) && rde_math_is_finite_f32(_print_y) && _print_x > 0.5f && _print_x < 2.0f && _print_y > 0.5f && _print_y < 2.0f) {
            _s.print_x = _print_x;
            _s.print_y = _print_y;
        }
        const f32 _kerf = fude_get_f32(&_c);   // Older files: the saw's kerf never set.
        if(_c.ok && rde_math_is_finite_f32(_kerf) && _kerf >= 0.0f && _kerf <= 50.0f) {
            _s.kerf_mm = _kerf;
        }
        const u8 _fps = fude_get_u8(&_c);   // Older files: the frame rate is not shown.
        if(_c.ok) { _s.show_fps = _fps != 0u; }
        const u8 _grain = fude_get_u8(&_c);   // Older files: fitted parts are free to turn.
        if(_c.ok) { _s.keep_grain = _grain != 0u; }
        const u8 _chosen = fude_get_u8(&_c);   // Older files: smoothing never chosen by hand.
        if(_c.ok) { _s.smooth_chosen = _chosen != 0u; }
        // Older files: their ML Kit byte is no choice (the canvas apps wrote 0 before they had the switch), so ML Kit
        // keeps the value the settings came in with: on, its default.
        const u8 _mlkit_chosen = fude_get_u8(&_c);
        if(_c.ok && _mlkit_chosen == 1u && _mlkit_read) { _s.mlkit = _mlkit != 0; }
    }

    *_settings = _s;
    return true;
}

// Loads the file at _path, or its backup, through fude_settings_read. See the header.
FUDE_LOAD_ fude_load_settings(const c8* _path, fude_settings* _settings) {
    return fude_save_load(_path, fude_settings_read, _settings);
}

// Whether two floats have the same bits. Comparing bits rather than values means a NaN equals itself, so a NaN never
// makes the session save again every time it checks.
RDE_INTERNAL b8 fude_same_f32(f32 _a, f32 _b) {
    return rde_memcmp(&_a, &_b, sizeof(f32)) == 0;
}

// Whether every field of _a equals _b's, floats by their bits. A new field must be added here too, or a change to it
// alone would not be saved.
b8 fude_settings_equal(const fude_settings* _a, const fude_settings* _b) {
    return _a->tool == _b->tool && _a->vertical == _b->vertical && _a->show_hud == _b->show_hud &&
           _a->brush_scale == _b->brush_scale && _a->width_mode == _b->width_mode &&
           _a->color.r == _b->color.r && _a->color.g == _b->color.g && _a->color.b == _b->color.b && _a->color.a == _b->color.a &&
           fude_same_f32(_a->radius, _b->radius) && rde_memcmp(&_a->marker_color, &_b->marker_color, sizeof(rde_color)) == 0 &&
           fude_same_f32(_a->marker_radius, _b->marker_radius) &&
           fude_same_f32(_a->toolbar_center.x, _b->toolbar_center.x) && fude_same_f32(_a->toolbar_center.y, _b->toolbar_center.y) &&
           _a->theme == _b->theme && _a->mlkit == _b->mlkit && _a->toolbar_minimized == _b->toolbar_minimized &&
           _a->paper_size == _b->paper_size && _a->language == _b->language && _a->finger_writes == _b->finger_writes &&
           _a->pen_ever == _b->pen_ever && _a->cards_read == _b->cards_read && _a->ui_size == _b->ui_size &&
           _a->smoothing == _b->smoothing && fude_same_f32(_a->eraser_radius, _b->eraser_radius) &&
           _a->units == _b->units && fude_same_f32(_a->true_mm_per_point, _b->true_mm_per_point) &&
           fude_same_f32(_a->print_x, _b->print_x) && fude_same_f32(_a->print_y, _b->print_y) && fude_same_f32(_a->kerf_mm, _b->kerf_mm) &&
           _a->show_fps == _b->show_fps && _a->keep_grain == _b->keep_grain && _a->smooth_chosen == _b->smooth_chosen;
}
