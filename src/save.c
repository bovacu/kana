#include "save.h"
#include "kfile.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See save.h.
// ===========================================================================

#define KANA_KIND_DOCUMENT  KANA_TAG('D', 'O', 'C', ' ')
#define KANA_KIND_SETTINGS  KANA_TAG('S', 'E', 'T', 'T')

#define KANA_CHUNK_VIEW     KANA_TAG('V', 'I', 'E', 'W')
#define KANA_CHUNK_STROKES  KANA_TAG('S', 'T', 'R', 'K')
#define KANA_CHUNK_PAGE     KANA_TAG('P', 'A', 'G', 'E')
#define KANA_CHUNK_POINTS   KANA_TAG('P', 'N', 'T', 'S')
#define KANA_CHUNK_PREFS    KANA_TAG('P', 'R', 'E', 'F')

#define KANA_STROKE_RECORD_SIZE 12u
#define KANA_POINT_RECORD_SIZE  20u

#define KANA_STROKE_FLAG_FROM_PEN 0x01u

// Before themes, the default ink was saved as this colour; it now means "the
// theme's ink" (KANA_THEME_INK), so old pages and settings follow the theme too.
#define KANA_LEGACY_INK (rde_color){ 30, 30, 36, 255 }

RDE_INTERNAL rde_color kana_saved_color(rde_color _c) {
    const rde_color _legacy = KANA_LEGACY_INK;
    return (_c.r == _legacy.r && _c.g == _legacy.g && _c.b == _legacy.b && _c.a == _legacy.a) ? KANA_THEME_INK : _c;
}

// --- files ---------------------------------------------------------------------

const c8* kana_save_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };

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
        snprintf(_dir, sizeof(_dir), "%s%skana/", _base, (_len > 0 && _base[_len - 1] == '/') ? "" : "/");
        _allocator->free(_allocator->allocator, (any)_base);
    }
#endif

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(_dir), "./saves/");
    }

    // Parents of a file path: create_missing_dirs strips the file name and leaves
    // existing folders alone. (rde_file_create_dir REPLACES an existing folder.)
    c8 _probe[RDE_MAX_PATH];
    snprintf(_probe, sizeof(_probe), "%s%s", _dir, KANA_SAVE_DOCUMENT_FILE);
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not create the save folder %s", _dir);
    }

    return _dir;
}

// --- document ------------------------------------------------------------------

b8 kana_save_document(const c8* _path, const kana_ink* _ink, kana_view _view, kana_page _page, u32* _out_bytes) {
    const u32 _total   = kana_ink_stroke_count(_ink);
    u32       _strokes = 0;
    u32       _points  = 0;
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(_stroke->alive && _stroke->point_count > 0) {
            _strokes++;
            _points += _stroke->point_count;
        }
    }

    // Sized up front (+1: rde_arr grows when an add reaches its capacity), so the
    // whole file is built without a single reallocation.
    kana_bytes _b = kana_bytes_new(KANA_FILE_HEADER_SIZE + 20u + 9u + (16u + _strokes * KANA_STROKE_RECORD_SIZE) + (16u + _points * KANA_POINT_RECORD_SIZE) + 1u);
    kana_put_header(&_b, KANA_SAVE_VERSION, KANA_KIND_DOCUMENT);

    u32 _chunk = kana_chunk_begin(&_b, KANA_CHUNK_VIEW);
    kana_put_f32(&_b, _view.offset.x);
    kana_put_f32(&_b, _view.offset.y);
    kana_put_f32(&_b, _view.zoom);
    kana_chunk_end(&_b, _chunk);

    _chunk = kana_chunk_begin(&_b, KANA_CHUNK_PAGE);
    kana_put_u8(&_b, (u8)_page.paper);
    kana_chunk_end(&_b, _chunk);

    _chunk = kana_chunk_begin(&_b, KANA_CHUNK_STROKES);
    kana_put_u32(&_b, _strokes);
    kana_put_u32(&_b, KANA_STROKE_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        kana_put_u32(&_b, _stroke->point_count);
        kana_put_color(&_b, _stroke->color);
        kana_put_u8(&_b, _stroke->from_pen ? KANA_STROKE_FLAG_FROM_PEN : 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
    }
    kana_chunk_end(&_b, _chunk);

    _chunk = kana_chunk_begin(&_b, KANA_CHUNK_POINTS);
    kana_put_u32(&_b, _points);
    kana_put_u32(&_b, KANA_POINT_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const kana_ink_point* _p = kana_ink_stroke_points(_ink, _stroke);
        for(u32 _i = 0; _i < _stroke->point_count; _i++) {
            kana_put_f32(&_b, _p[_i].position.x);
            kana_put_f32(&_b, _p[_i].position.y);
            kana_put_f32(&_b, _p[_i].pressure);
            kana_put_f32(&_b, _p[_i].radius);
            kana_put_f32(&_b, _p[_i].time);
        }
    }
    kana_chunk_end(&_b, _chunk);

    return kana_bytes_write_and_free(&_b, _path, _out_bytes);
}

RDE_INTERNAL b8 kana_finite(f32 _v) {
    return isfinite(_v) != 0;
}

typedef struct {
    u32       point_count;
    rde_color color;
    b8        from_pen;
} kana_loaded_stroke;

// Parses the whole document before touching _ink, so a damaged file changes nothing.
RDE_INTERNAL b8 kana_parse_document(const u8* _data, u32 _size, kana_ink* _ink, kana_view* _view, kana_page* _page) {
    kana_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = true };
    if(!kana_read_header(&_r, KANA_SAVE_VERSION, KANA_KIND_DOCUMENT)) {
        return false;
    }

    kana_view           _v            = *_view;
    kana_page           _pg           = { 0 };   // a file from before 'PAGE': the dots
    kana_loaded_stroke* _strokes      = NULL;
    u32                 _stroke_count = 0;
    kana_ink_point*     _points       = NULL;
    u32                 _point_count  = 0;
    b8                  _have_strokes = false;
    b8                  _have_points  = false;
    b8                  _ok           = true;

    while(_ok && _r.pos < _r.size) {
        const u32 _tag  = kana_get_u32(&_r);
        const u32 _len  = kana_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            _ok = false;
            break;
        }

        // Each chunk is read through its own window, so a short or long payload
        // can never run into the next chunk.
        kana_reader _c   = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos          += _len;

        if(_tag == KANA_CHUNK_VIEW) {
            const f32 _x = kana_get_f32(&_c);
            const f32 _y = kana_get_f32(&_c);
            const f32 _z = kana_get_f32(&_c);
            if(_c.ok && kana_finite(_x) && kana_finite(_y) && kana_finite(_z) && _z > 0.0f) {
                _v.offset = (rde_vec_2F){ _x, _y };
                _v.zoom   = rde_math_clamp_f32(_z, KANA_CANVAS_ZOOM_MIN, KANA_CANVAS_ZOOM_MAX);
            }
        } else if(_tag == KANA_CHUNK_PAGE) {
            const u8 _paper = kana_get_u8(&_c);
            if(_c.ok && _paper < KANA_PAPER_COUNT) {
                _pg.paper = (KANA_PAPER_)_paper;
            }
        } else if(_tag == KANA_CHUNK_STROKES && !_have_strokes) {
            const u32 _count  = kana_get_u32(&_c);
            const u32 _record = kana_get_u32(&_c);
            if(!_c.ok || _record < KANA_STROKE_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _strokes = _count > 0 ? rde_malloc((usize)_count * sizeof(kana_loaded_stroke)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                _strokes[_i].point_count = kana_get_u32(&_c);
                _strokes[_i].color       = kana_saved_color(kana_get_color(&_c));
                _strokes[_i].from_pen    = (kana_get_u8(&_c) & KANA_STROKE_FLAG_FROM_PEN) != 0;
                _c.pos = _start + _record;   // skip fields a newer build added
                if(_strokes[_i].point_count == 0) {
                    _ok = false;
                }
            }
            _stroke_count = _count;
            _have_strokes = true;
        } else if(_tag == KANA_CHUNK_POINTS && !_have_points) {
            const u32 _count  = kana_get_u32(&_c);
            const u32 _record = kana_get_u32(&_c);
            if(!_c.ok || _record < KANA_POINT_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _points = _count > 0 ? rde_malloc((usize)_count * sizeof(kana_ink_point)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                kana_ink_point* _p = &_points[_i];
                _p->position.x = kana_get_f32(&_c);
                _p->position.y = kana_get_f32(&_c);
                _p->pressure   = kana_get_f32(&_c);
                _p->radius     = kana_get_f32(&_c);
                _p->time       = kana_get_f32(&_c);
                _c.pos = _start + _record;

                if(!kana_finite(_p->position.x) || !kana_finite(_p->position.y)) {
                    _ok = false;
                }
                // Cosmetic fields: repaired rather than rejected.
                _p->pressure = kana_finite(_p->pressure) ? rde_math_clamp_f32(_p->pressure, 0.0f, 1.0f) : 0.0f;
                _p->radius   = (kana_finite(_p->radius) && _p->radius > 0.0f) ? _p->radius : KANA_INK_RADIUS_DEFAULT;
                _p->time     = kana_finite(_p->time) ? _p->time : 0.0f;
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
            kana_ink_add_loaded_stroke(_ink, &_points[_first], _strokes[_i].point_count, _strokes[_i].color, _strokes[_i].from_pen);
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
RDE_INTERNAL KANA_LOAD_ kana_load_document_file(const c8* _file, kana_ink* _ink, kana_view* _view, kana_page* _page) {
    if(!rde_file_exists(_file)) {
        return KANA_LOAD_MISSING;
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_file, &_size);
    const b8 _ok = _data != NULL && kana_parse_document(_data, _size, _ink, _view, _page);
    kana_file_free(_data);

    if(!_ok) {
        kana_file_set_aside(_file);
        return KANA_LOAD_CORRUPT;
    }

    return KANA_LOAD_OK;
}

KANA_LOAD_ kana_load_document(const c8* _path, kana_ink* _ink, kana_view* _view, kana_page* _page) {
    const KANA_LOAD_ _main = kana_load_document_file(_path, _ink, _view, _page);
    if(_main == KANA_LOAD_OK) {
        return KANA_LOAD_OK;
    }

    // Missing (a kill between a save's two renames) or damaged: the backup is the
    // previous save.
    c8 _bak[RDE_MAX_PATH];
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    const KANA_LOAD_ _backup = kana_load_document_file(_bak, _ink, _view, _page);
    if(_backup == KANA_LOAD_OK) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: %s %s, loaded the backup", _path, _main == KANA_LOAD_MISSING ? "missing" : "damaged");
        return KANA_LOAD_RECOVERED;
    }

    return (_main == KANA_LOAD_CORRUPT || _backup == KANA_LOAD_CORRUPT) ? KANA_LOAD_CORRUPT : KANA_LOAD_MISSING;
}

// --- settings --------------------------------------------------------------------

b8 kana_save_settings(const c8* _path, const kana_settings* _settings) {
    kana_bytes _b = kana_bytes_new(64u);
    kana_put_header(&_b, KANA_SAVE_VERSION, KANA_KIND_SETTINGS);

    const u32 _chunk = kana_chunk_begin(&_b, KANA_CHUNK_PREFS);
    kana_put_u8(&_b, _settings->tool);
    kana_put_u8(&_b, _settings->vertical ? 1u : 0u);
    kana_put_u8(&_b, _settings->show_hud ? 1u : 0u);
    kana_put_u8(&_b, _settings->brush_scale);
    kana_put_u8(&_b, _settings->width_mode);
    kana_put_color(&_b, _settings->color);
    kana_put_f32(&_b, _settings->radius);
    kana_put_f32(&_b, _settings->toolbar_center.x);
    kana_put_f32(&_b, _settings->toolbar_center.y);
    kana_put_u8(&_b, _settings->theme);
    kana_put_u8(&_b, _settings->mlkit ? 1u : 0u);
    kana_put_u8(&_b, _settings->toolbar_minimized ? 1u : 0u);
    kana_put_u8(&_b, _settings->paper_size);
    kana_put_u8(&_b, _settings->language);
    kana_chunk_end(&_b, _chunk);

    return kana_bytes_write_and_free(&_b, _path, NULL);
}

KANA_LOAD_ kana_load_settings(const c8* _path, kana_settings* _settings) {
    // Settings are small and replaceable: the main file, else the backup, and a
    // damaged one is simply set aside.
    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return KANA_LOAD_MISSING;
        }
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_from, &_size);

    kana_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = _data != NULL };
    if(_data == NULL || !kana_read_header(&_r, KANA_SAVE_VERSION, KANA_KIND_SETTINGS)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return KANA_LOAD_CORRUPT;
    }

    kana_settings _s = *_settings;

    while(_r.ok && _r.pos < _r.size) {
        const u32 _tag = kana_get_u32(&_r);
        const u32 _len = kana_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            break;
        }

        kana_reader _c  = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos         += _len;

        if(_tag != KANA_CHUNK_PREFS) {
            continue;
        }

        // Field by field, in order: a file from an older build simply ends early,
        // and each field it has is taken only if it is in range.
        const u8 _tool = kana_get_u8(&_c);
        if(_c.ok && _tool <= 2u) { _s.tool = _tool; }
        const u8 _vertical = kana_get_u8(&_c);
        if(_c.ok) { _s.vertical = _vertical != 0; }
        const u8 _hud = kana_get_u8(&_c);
        if(_c.ok) { _s.show_hud = _hud != 0; }
        const u8 _scale = kana_get_u8(&_c);
        if(_c.ok && _scale <= 1u) { _s.brush_scale = _scale; }
        const u8 _width = kana_get_u8(&_c);
        if(_c.ok && _width <= 1u) { _s.width_mode = _width; }
        const rde_color _color = kana_get_color(&_c);
        if(_c.ok) { _s.color = kana_saved_color(_color); }
        const f32 _radius = kana_get_f32(&_c);
        if(_c.ok && kana_finite(_radius) && _radius > 0.0f) { _s.radius = _radius; }
        const f32 _cx = kana_get_f32(&_c);
        const f32 _cy = kana_get_f32(&_c);
        if(_c.ok && kana_finite(_cx) && kana_finite(_cy)) { _s.toolbar_center = (rde_vec_2F){ _cx, _cy }; }
        const u8 _theme = kana_get_u8(&_c);
        if(_c.ok && _theme < KANA_THEME_COUNT) { _s.theme = _theme; }
        const u8 _mlkit = kana_get_u8(&_c);
        if(_c.ok && _mlkit <= 1u) { _s.mlkit = _mlkit != 0; }
        const u8 _minimized = kana_get_u8(&_c);
        if(_c.ok && _minimized <= 1u) { _s.toolbar_minimized = _minimized != 0; }
        const u8 _paper = kana_get_u8(&_c);
        if(_c.ok && _paper < KANA_PAPER_SIZE_COUNT) { _s.paper_size = _paper; }
        const u8 _language = kana_get_u8(&_c);
        if(_c.ok && _language < RDE_LANGUAGE_COUNT) { _s.language = _language; }
    }

    kana_file_free(_data);
    *_settings = _s;
    return KANA_LOAD_OK;
}

RDE_INTERNAL b8 kana_same_f32(f32 _a, f32 _b) {
    return memcmp(&_a, &_b, sizeof(f32)) == 0;
}

b8 kana_settings_equal(const kana_settings* _a, const kana_settings* _b) {
    return _a->tool == _b->tool && _a->vertical == _b->vertical && _a->show_hud == _b->show_hud &&
           _a->brush_scale == _b->brush_scale && _a->width_mode == _b->width_mode &&
           _a->color.r == _b->color.r && _a->color.g == _b->color.g && _a->color.b == _b->color.b && _a->color.a == _b->color.a &&
           kana_same_f32(_a->radius, _b->radius) &&
           kana_same_f32(_a->toolbar_center.x, _b->toolbar_center.x) && kana_same_f32(_a->toolbar_center.y, _b->toolbar_center.y) &&
           _a->theme == _b->theme && _a->mlkit == _b->mlkit && _a->toolbar_minimized == _b->toolbar_minimized &&
           _a->paper_size == _b->paper_size && _a->language == _b->language;
}
