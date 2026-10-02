#include "drawing/base/save.h"
#include "drawing/base/kfile.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See save.h.
// ===========================================================================

#define FUDE_KIND_DOCUMENT  FUDE_TAG('D', 'O', 'C', ' ')
#define FUDE_KIND_SETTINGS  FUDE_TAG('S', 'E', 'T', 'T')

#define FUDE_CHUNK_VIEW     FUDE_TAG('V', 'I', 'E', 'W')
#define FUDE_CHUNK_STROKES  FUDE_TAG('S', 'T', 'R', 'K')
#define FUDE_CHUNK_PAGE     FUDE_TAG('P', 'A', 'G', 'E')
#define FUDE_CHUNK_POINTS   FUDE_TAG('P', 'N', 'T', 'S')
#define FUDE_CHUNK_PREFS    FUDE_TAG('P', 'R', 'E', 'F')

#define FUDE_STROKE_RECORD_SIZE 12u
#define FUDE_POINT_RECORD_SIZE  20u

#define FUDE_STROKE_FLAG_FROM_PEN 0x01u
#define FUDE_STROKE_FLAG_MARKER   0x02u   // the marker's (an older build draws it as ink of its colour)

// Before themes, the default ink was saved as this colour; it now means "the
// theme's ink" (FUDE_THEME_INK), so old pages and settings follow the theme too.
#define FUDE_LEGACY_INK (rde_color){ 30, 30, 36, 255 }

RDE_INTERNAL rde_color fude_saved_color(rde_color _c) {
    const rde_color _legacy = FUDE_LEGACY_INK;
    return (_c.r == _legacy.r && _c.g == _legacy.g && _c.b == _legacy.b && _c.a == _legacy.a) ? FUDE_THEME_INK : _c;
}

// --- files ---------------------------------------------------------------------

// The folder's name in the app's own storage (a phone's or tablet's): the app's
// (the session sets it from the app's id, info.h), and the folder found from it.
RDE_INTERNAL c8 fude_save_folder[64] = "fude";
RDE_INTERNAL c8 fude_save_dir_found[RDE_MAX_PATH];

void fude_save_set_folder(const c8* _name) {
    if(_name != NULL && _name[0] != 0 && strcmp(_name, fude_save_folder) != 0) {
        snprintf(fude_save_folder, sizeof(fude_save_folder), "%s", _name);
        fude_save_dir_found[0] = 0;   // found again, in it
    }
}

const c8* fude_save_dir(void) {
    c8* const _dir = fude_save_dir_found;

    if(_dir[0] != 0) {
        return _dir;
    }

#if defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_ANDROID)
    rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
#if defined(RDE_PLATFORM_IOS)
    const c8* _base = rde_ios_get_internal_storage_data_path(_allocator);
#else
    const c8* _base = rde_android_get_internal_storage_data_path(_allocator);
#endif
    if(_base != NULL) {
        const usize _len = strlen(_base);
        snprintf(_dir, sizeof(fude_save_dir_found), "%s%s%s/", _base, (_len > 0 && _base[_len - 1] == '/') ? "" : "/", fude_save_folder);
        _allocator->free(_allocator->allocator, (any)_base);
    }
#endif

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(fude_save_dir_found), "./saves/");
    }

    // Parents of a file path: create_missing_dirs strips the file name and leaves
    // existing folders alone. (rde_file_create_dir REPLACES an existing folder.)
    c8 _probe[RDE_MAX_PATH];
    snprintf(_probe, sizeof(_probe), "%s%s", _dir, FUDE_SAVE_DOCUMENT_FILE);
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: could not create the save folder %s", _dir);
    }

    return _dir;
}

// --- document ------------------------------------------------------------------

b8 fude_save_document(const c8* _path, const fude_ink* _ink, fude_view _view, fude_page _page, u32* _out_bytes) {
    const u32 _total   = fude_ink_stroke_count(_ink);
    u32       _strokes = 0;
    u32       _points  = 0;
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(_stroke->alive && _stroke->point_count > 0) {
            _strokes++;
            _points += _stroke->point_count;
        }
    }

    // Sized up front (+1: rde_arr grows when an add reaches its capacity), so the
    // whole file is built without a single reallocation.
    fude_bytes _b = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 20u + 9u + (16u + _strokes * FUDE_STROKE_RECORD_SIZE) + (16u + _points * FUDE_POINT_RECORD_SIZE) + 1u);
    fude_put_header(&_b, FUDE_SAVE_VERSION, FUDE_KIND_DOCUMENT);

    u32 _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_VIEW);
    fude_put_f32(&_b, _view.offset.x);
    fude_put_f32(&_b, _view.offset.y);
    fude_put_f32(&_b, _view.zoom);
    fude_chunk_end(&_b, _chunk);

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_PAGE);
    fude_put_u8(&_b, (u8)_page.paper);
    fude_chunk_end(&_b, _chunk);

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_STROKES);
    fude_put_u32(&_b, _strokes);
    fude_put_u32(&_b, FUDE_STROKE_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        fude_put_u32(&_b, _stroke->point_count);
        fude_put_color(&_b, _stroke->color);
        fude_put_u8(&_b, (u8)((_stroke->from_pen ? FUDE_STROKE_FLAG_FROM_PEN : 0u) | (_stroke->marker ? FUDE_STROKE_FLAG_MARKER : 0u)));
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
    }
    fude_chunk_end(&_b, _chunk);

    _chunk = fude_chunk_begin(&_b, FUDE_CHUNK_POINTS);
    fude_put_u32(&_b, _points);
    fude_put_u32(&_b, FUDE_POINT_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const fude_ink_point* _p = fude_ink_stroke_points(_ink, _stroke);
        for(u32 _i = 0; _i < _stroke->point_count; _i++) {
            fude_put_f32(&_b, _p[_i].position.x);
            fude_put_f32(&_b, _p[_i].position.y);
            fude_put_f32(&_b, _p[_i].pressure);
            fude_put_f32(&_b, _p[_i].radius);
            fude_put_f32(&_b, _p[_i].time);
        }
    }
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, _out_bytes);
}

RDE_INTERNAL b8 fude_finite(f32 _v) {
    return isfinite(_v) != 0;
}

typedef struct {
    u32       point_count;
    rde_color color;
    b8        from_pen;
    b8        marker;
} fude_loaded_stroke;

// Parses the whole document before touching _ink, so a damaged file changes nothing.
RDE_INTERNAL b8 fude_parse_document(const u8* _data, u32 _size, fude_ink* _ink, fude_view* _view, fude_page* _page) {
    fude_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = true };
    if(!fude_read_header(&_r, FUDE_SAVE_VERSION, FUDE_KIND_DOCUMENT)) {
        return false;
    }

    fude_view           _v            = *_view;
    fude_page           _pg           = { 0 };   // a file from before 'PAGE': the dots
    fude_loaded_stroke* _strokes      = NULL;
    u32                 _stroke_count = 0;
    fude_ink_point*     _points       = NULL;
    u32                 _point_count  = 0;
    b8                  _have_strokes = false;
    b8                  _have_points  = false;
    b8                  _ok           = true;

    while(_ok && _r.pos < _r.size) {
        const u32 _tag  = fude_get_u32(&_r);
        const u32 _len  = fude_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            _ok = false;
            break;
        }

        // Each chunk is read through its own window, so a short or long payload
        // can never run into the next chunk.
        fude_reader _c   = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos          += _len;

        if(_tag == FUDE_CHUNK_VIEW) {
            const f32 _x = fude_get_f32(&_c);
            const f32 _y = fude_get_f32(&_c);
            const f32 _z = fude_get_f32(&_c);
            if(_c.ok && fude_finite(_x) && fude_finite(_y) && fude_finite(_z) && _z > 0.0f) {
                _v.offset = (rde_vec_2F){ _x, _y };
                _v.zoom   = rde_math_clamp_f32(_z, FUDE_CANVAS_ZOOM_MIN, FUDE_CANVAS_ZOOM_MAX);
            }
        } else if(_tag == FUDE_CHUNK_PAGE) {
            const u8 _paper = fude_get_u8(&_c);
            if(_c.ok && _paper < FUDE_PAPER_COUNT) {
                _pg.paper = (FUDE_PAPER_)_paper;
            }
        } else if(_tag == FUDE_CHUNK_STROKES && !_have_strokes) {
            const u32 _count  = fude_get_u32(&_c);
            const u32 _record = fude_get_u32(&_c);
            if(!_c.ok || _record < FUDE_STROKE_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _strokes = _count > 0 ? rde_malloc((usize)_count * sizeof(fude_loaded_stroke)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                _strokes[_i].point_count = fude_get_u32(&_c);
                _strokes[_i].color       = fude_saved_color(fude_get_color(&_c));
                const u8 _flags          = fude_get_u8(&_c);
                _strokes[_i].from_pen    = (_flags & FUDE_STROKE_FLAG_FROM_PEN) != 0;
                _strokes[_i].marker      = (_flags & FUDE_STROKE_FLAG_MARKER) != 0;
                _c.pos = _start + _record;   // skip fields a newer build added
                if(_strokes[_i].point_count == 0) {
                    _ok = false;
                }
            }
            _stroke_count = _count;
            _have_strokes = true;
        } else if(_tag == FUDE_CHUNK_POINTS && !_have_points) {
            const u32 _count  = fude_get_u32(&_c);
            const u32 _record = fude_get_u32(&_c);
            if(!_c.ok || _record < FUDE_POINT_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _points = _count > 0 ? rde_malloc((usize)_count * sizeof(fude_ink_point)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                fude_ink_point* _p = &_points[_i];
                _p->position.x = fude_get_f32(&_c);
                _p->position.y = fude_get_f32(&_c);
                _p->pressure   = fude_get_f32(&_c);
                _p->radius     = fude_get_f32(&_c);
                _p->time       = fude_get_f32(&_c);
                _c.pos = _start + _record;

                if(!fude_finite(_p->position.x) || !fude_finite(_p->position.y)) {
                    _ok = false;
                }
                // Cosmetic fields: repaired rather than rejected.
                _p->pressure = fude_finite(_p->pressure) ? rde_math_clamp_f32(_p->pressure, 0.0f, 1.0f) : 0.0f;
                _p->radius   = (fude_finite(_p->radius) && _p->radius > 0.0f) ? _p->radius : FUDE_INK_RADIUS_DEFAULT;
                _p->time     = fude_finite(_p->time) ? _p->time : 0.0f;
            }
            _point_count = _count;
            _have_points = true;
        }
        // Anything else: a chunk from a newer build. Skipped.
    }

    // The strokes must account for exactly the points there are.
    if(_ok) {
        u64 _sum = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            _sum += _strokes[_i].point_count;
        }
        _ok = _r.ok && _have_strokes == _have_points && _sum == _point_count;
    }

    if(_ok) {
        u32 _first = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            fude_ink_add_loaded_stroke_2(_ink, &_points[_first], _strokes[_i].point_count, _strokes[_i].color, _strokes[_i].from_pen, _strokes[_i].marker);
            _first += _strokes[_i].point_count;
        }
        *_view = _v;
        *_page = _pg;
    }

    rde_free(_strokes);
    rde_free(_points);
    return _ok;
}

// One file: OK, MISSING, or CORRUPT (and then set aside).
RDE_INTERNAL FUDE_LOAD_ fude_load_document_file(const c8* _file, fude_ink* _ink, fude_view* _view, fude_page* _page) {
    if(!rde_file_exists(_file)) {
        return FUDE_LOAD_MISSING;
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_file, &_size);
    const b8 _ok = _data != NULL && fude_parse_document(_data, _size, _ink, _view, _page);
    fude_file_free(_data);

    if(!_ok) {
        fude_file_set_aside(_file);
        return FUDE_LOAD_CORRUPT;
    }

    return FUDE_LOAD_OK;
}

FUDE_LOAD_ fude_load_document(const c8* _path, fude_ink* _ink, fude_view* _view, fude_page* _page) {
    const FUDE_LOAD_ _main = fude_load_document_file(_path, _ink, _view, _page);
    if(_main == FUDE_LOAD_OK) {
        return FUDE_LOAD_OK;
    }

    // Missing (a kill between a save's two renames) or damaged: the backup is the
    // previous save.
    c8 _bak[RDE_MAX_PATH];
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    const FUDE_LOAD_ _backup = fude_load_document_file(_bak, _ink, _view, _page);
    if(_backup == FUDE_LOAD_OK) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: %s %s, loaded the backup", _path, _main == FUDE_LOAD_MISSING ? "missing" : "damaged");
        return FUDE_LOAD_RECOVERED;
    }

    return (_main == FUDE_LOAD_CORRUPT || _backup == FUDE_LOAD_CORRUPT) ? FUDE_LOAD_CORRUPT : FUDE_LOAD_MISSING;
}

// --- settings --------------------------------------------------------------------

b8 fude_save_settings(const c8* _path, const fude_settings* _settings) {
    fude_bytes _b = fude_bytes_new(64u);
    fude_put_header(&_b, FUDE_SAVE_VERSION, FUDE_KIND_SETTINGS);

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
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, NULL);
}

FUDE_LOAD_ fude_load_settings(const c8* _path, fude_settings* _settings) {
    // Settings are small and replaceable: the main file, else the backup, and a
    // damaged one is simply set aside.
    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return FUDE_LOAD_MISSING;
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_from, &_size);

    fude_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = _data != NULL };
    if(_data == NULL || !fude_read_header(&_r, FUDE_SAVE_VERSION, FUDE_KIND_SETTINGS)) {
        fude_file_free(_data);
        fude_file_set_aside(_from);
        return FUDE_LOAD_CORRUPT;
    }

    fude_settings _s = *_settings;

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

        // Field by field, in order: a file from an older build simply ends early,
        // and each field it has is taken only if it is in range.
        const u8 _tool = fude_get_u8(&_c);
        if(_c.ok && _tool <= 3u) { _s.tool = _tool; }   // FUDE_TOOL_: draw, erase, lasso, mark
        const u8 _vertical = fude_get_u8(&_c);
        if(_c.ok) { _s.vertical = _vertical != 0; }
        const u8 _hud = fude_get_u8(&_c);
        if(_c.ok) { _s.show_hud = _hud != 0; }
        const u8 _scale = fude_get_u8(&_c);
        if(_c.ok && _scale <= 1u) { _s.brush_scale = _scale; }
        const u8 _width = fude_get_u8(&_c);
        if(_c.ok && _width <= 1u) { _s.width_mode = _width; }
        const rde_color _color = fude_get_color(&_c);
        if(_c.ok) { _s.color = fude_saved_color(_color); }
        const f32 _radius = fude_get_f32(&_c);
        if(_c.ok && fude_finite(_radius) && _radius > 0.0f) { _s.radius = _radius; }
        const f32 _cx = fude_get_f32(&_c);
        const f32 _cy = fude_get_f32(&_c);
        if(_c.ok && fude_finite(_cx) && fude_finite(_cy)) { _s.toolbar_center = (rde_vec_2F){ _cx, _cy }; }
        const u8 _theme = fude_get_u8(&_c);
        if(_c.ok && _theme < FUDE_THEME_COUNT) { _s.theme = _theme; }
        const u8 _mlkit = fude_get_u8(&_c);
        if(_c.ok && _mlkit <= 1u) { _s.mlkit = _mlkit != 0; }
        const u8 _minimized = fude_get_u8(&_c);
        if(_c.ok && _minimized <= 1u) { _s.toolbar_minimized = _minimized != 0; }
        const u8 _paper = fude_get_u8(&_c);
        if(_c.ok && _paper < FUDE_PAPER_SIZE_COUNT) { _s.paper_size = _paper; }
        const u8 _language = fude_get_u8(&_c);
        if(_c.ok && _language < RDE_LANGUAGE_COUNT) { _s.language = _language; }
        // From before the toolbar's hand: a pen user's (the pen writes, fingers move the page).
        _s.finger_writes = false;
        _s.pen_ever      = true;
        const u8 _finger = fude_get_u8(&_c);
        if(_c.ok && _finger <= 1u) { _s.finger_writes = _finger != 0; }
        const u8 _pen = fude_get_u8(&_c);
        if(_c.ok && _pen <= 1u) { _s.pen_ever = _pen != 0; }
        const rde_color _marker = fude_get_color(&_c);
        if(_c.ok && _marker.a > 0u) { _s.marker_color = _marker; }
        const f32 _marker_radius = fude_get_f32(&_c);
        if(_c.ok && fude_finite(_marker_radius) && _marker_radius > 0.0f) { _s.marker_radius = _marker_radius; }
    }

    fude_file_free(_data);
    *_settings = _s;
    return FUDE_LOAD_OK;
}

RDE_INTERNAL b8 fude_same_f32(f32 _a, f32 _b) {
    return memcmp(&_a, &_b, sizeof(f32)) == 0;
}

b8 fude_settings_equal(const fude_settings* _a, const fude_settings* _b) {
    return _a->tool == _b->tool && _a->vertical == _b->vertical && _a->show_hud == _b->show_hud &&
           _a->brush_scale == _b->brush_scale && _a->width_mode == _b->width_mode &&
           _a->color.r == _b->color.r && _a->color.g == _b->color.g && _a->color.b == _b->color.b && _a->color.a == _b->color.a &&
           fude_same_f32(_a->radius, _b->radius) && memcmp(&_a->marker_color, &_b->marker_color, sizeof(rde_color)) == 0 &&
           fude_same_f32(_a->marker_radius, _b->marker_radius) &&
           fude_same_f32(_a->toolbar_center.x, _b->toolbar_center.x) && fude_same_f32(_a->toolbar_center.y, _b->toolbar_center.y) &&
           _a->theme == _b->theme && _a->mlkit == _b->mlkit && _a->toolbar_minimized == _b->toolbar_minimized &&
           _a->paper_size == _b->paper_size && _a->language == _b->language && _a->finger_writes == _b->finger_writes &&
           _a->pen_ever == _b->pen_ever;
}
