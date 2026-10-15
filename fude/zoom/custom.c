// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/custom.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See custom.h.
//
// The map is one array in the order the templates were set, searched in full by key or by canvas; there are as many
// as a person has kept pieces and made parts. The internal helper is named fzct_.
// ===========================================================================

// See the header.
void fude_zoom_templates_init(fude_zoom_templates* _t) {
    _t->list = rde_arr_new(sizeof(fude_zoom_template), rde_memory_allocator_get_default_std());
}

// See the header.
void fude_zoom_templates_free(fude_zoom_templates* _t) {
    rde_arr_free(&_t->list);
}

// See the header. Each line is read through fude_zoom_templates_set, so a key it would refuse is skipped; so are a
// line of FUDE_ZOOM_CUSTOM_KEY + 32 bytes or more, an unknown kind and canvas 0.
void fude_zoom_templates_read(fude_zoom_templates* _t, const c8* _path) {
    rde_arr_clear(&_t->list);
    if(!rde_file_exists(_path)) {
        return;   // (none yet: not a failure to read)
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        // (there, but not read: no memory, 4 GiB or more, or gone since; every template is made again when opened)
        rde_log_level(RDE_LOG_LEVEL_WARNING, "custom: %s is there but could not be read; no template canvas is known", _path);
        return;
    }
    const c8* _at  = (const c8*)_data;
    const c8* _end = _at + _size;
    while(_at < _end) {
        const c8* _eol = rde_memchr(_at, '\n', (usize)(_end - _at));
        const usize _n = (usize)((_eol != NULL ? _eol : _end) - _at);
        c8 _line[FUDE_ZOOM_CUSTOM_KEY + 32u];
        if(_n < sizeof(_line)) {
            rde_memcpy(_line, _at, _n);
            _line[_n] = 0;
            c8 _kind = 0;
            c8 _key[FUDE_ZOOM_CUSTOM_KEY];
            unsigned _canvas = 0;
            if(rde_sscanf(_line, "%c %63s %u", &_kind, _key, &_canvas) == 3 && (_kind == 'p' || _kind == 'u') && _canvas != 0u) {
                fude_zoom_templates_set(_t, _kind == 'p' ? FUDE_ZOOM_CUSTOM_PIECE : FUDE_ZOOM_CUSTOM_PART, _key, (u32)_canvas);
            }
        }
        _at += _n + 1u;
    }
    fude_file_free(_data);
}

// See the header: the whole map, one line for each template, in the map's order.
b8 fude_zoom_templates_write(const fude_zoom_templates* _t, const c8* _path) {
    fude_bytes _out = fude_bytes_new(256u);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->list); _i++) {
        const fude_zoom_template* _e = &((const fude_zoom_template*)_t->list.memory)[_i];
        c8 _line[FUDE_ZOOM_CUSTOM_KEY + 32u];
        rde_snprintf(_line, sizeof(_line), "%c %s %u\n", _e->kind == FUDE_ZOOM_CUSTOM_PIECE ? 'p' : 'u', _e->key, _e->canvas);
        fude_put_data(&_out, _line, (u32)rde_strlen(_line));
    }
    return fude_bytes_write_and_free(&_out, _path, NULL);
}

// Returns _kind's _key's entry in the map, or NULL.
RDE_INTERNAL fude_zoom_template* fzct_find(const fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    fude_zoom_template* _e = (fude_zoom_template*)_t->list.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->list); _i++) {
        if(_e[_i].kind == _kind && rde_strcmp(_e[_i].key, _key) == 0) {
            return &_e[_i];
        }
    }
    return NULL;
}

// See the header.
u32 fude_zoom_templates_canvas(const fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    const fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    return _e != NULL ? _e->canvas : 0u;
}

// See the header.
const fude_zoom_template* fude_zoom_templates_of(const fude_zoom_templates* _t, u32 _canvas) {
    const fude_zoom_template* _e = (const fude_zoom_template*)_t->list.memory;
    for(u32 _i = 0; _canvas != 0u && _i < (u32)rde_arr_length(&_t->list); _i++) {
        if(_e[_i].canvas == _canvas) {
            return &_e[_i];
        }
    }
    return NULL;
}

// Returns whether _key can be a key: not NULL, not empty, short enough, and with no space (a key with a space would not
// be read back from the file: none is made so — a piece's is a number, a part's a slug).
RDE_INTERNAL b8 fzct_key_ok(const c8* _key) {
    return _key != NULL && _key[0] != 0 && rde_strlen(_key) < FUDE_ZOOM_CUSTOM_KEY && rde_strchr(_key, ' ') == NULL;
}

// See the header.
void fude_zoom_templates_set(fude_zoom_templates* _t, u8 _kind, const c8* _key, u32 _canvas) {
    if(!fzct_key_ok(_key)) {
        return;
    }
    fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    if(_e != NULL) {
        _e->canvas = _canvas;
        return;
    }
    fude_zoom_template _new = { _kind, { 0 }, _canvas };
    rde_snprintf(_new.key, sizeof(_new.key), "%s", _key);
    rde_arr_add(&_t->list, (any)&_new);
}

// See the header.
u32 fude_zoom_templates_drop(fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    if(_e == NULL) {
        return 0u;
    }
    const u32 _canvas = _e->canvas;
    rde_arr_remove(&_t->list, (u32)(_e - (fude_zoom_template*)_t->list.memory));
    return _canvas;
}

// See the header. Everything is checked before anything changes, so a refused move loses no template: a _to that
// could not be a key would have lost _key's, and a _to with a template of its own would have had its canvas left
// belonging to nobody. The entry keeps its place in the map, only its key renamed.
b8 fude_zoom_templates_rekey(fude_zoom_templates* _t, u8 _kind, const c8* _key, const c8* _to) {
    fude_zoom_template* _e = _key != NULL ? fzct_find(_t, _kind, _key) : NULL;
    if(_e == NULL || !fzct_key_ok(_to)) {
        return false;
    }
    if(rde_strcmp(_key, _to) == 0) {
        return true;   // (already there)
    }
    if(fzct_find(_t, _kind, _to) != NULL) {
        return false;
    }
    rde_snprintf(_e->key, sizeof(_e->key), "%s", _to);
    return true;
}
