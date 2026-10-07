// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/custom.h"
#include "drawing/base/kfile.h"
#include <stdio.h>
#include <string.h>

void fude_zoom_templates_init(fude_zoom_templates* _t) {
    _t->list = rde_arr_new(sizeof(fude_zoom_template), rde_memory_allocator_get_default_std());
}

void fude_zoom_templates_free(fude_zoom_templates* _t) {
    rde_arr_free(&_t->list);
}

void fude_zoom_templates_read(fude_zoom_templates* _t, const c8* _path) {
    rde_arr_clear(&_t->list);
    if(!rde_file_exists(_path)) {
        return;   // (none yet: not a failure to read)
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return;
    }
    const c8* _at  = (const c8*)_data;
    const c8* _end = _at + _size;
    while(_at < _end) {
        const c8* _eol = memchr(_at, '\n', (usize)(_end - _at));
        const usize _n = (usize)((_eol != NULL ? _eol : _end) - _at);
        c8 _line[FUDE_ZOOM_CUSTOM_KEY + 32u];
        if(_n < sizeof(_line)) {
            memcpy(_line, _at, _n);
            _line[_n] = 0;
            c8 _kind = 0;
            c8 _key[FUDE_ZOOM_CUSTOM_KEY];
            unsigned _canvas = 0;
            if(sscanf(_line, "%c %63s %u", &_kind, _key, &_canvas) == 3 && (_kind == 'p' || _kind == 'u') && _canvas != 0u) {
                fude_zoom_templates_set(_t, _kind == 'p' ? FUDE_ZOOM_CUSTOM_PIECE : FUDE_ZOOM_CUSTOM_PART, _key, (u32)_canvas);
            }
        }
        _at += _n + 1u;
    }
    fude_file_free(_data);
}

b8 fude_zoom_templates_write(const fude_zoom_templates* _t, const c8* _path) {
    fude_bytes _out = fude_bytes_new(256u);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->list); _i++) {
        const fude_zoom_template* _e = &((const fude_zoom_template*)_t->list.memory)[_i];
        c8 _line[FUDE_ZOOM_CUSTOM_KEY + 32u];
        snprintf(_line, sizeof(_line), "%c %s %u\n", _e->kind == FUDE_ZOOM_CUSTOM_PIECE ? 'p' : 'u', _e->key, _e->canvas);
        fude_put_data(&_out, _line, (u32)strlen(_line));
    }
    return fude_bytes_write_and_free(&_out, _path, NULL);
}

RDE_INTERNAL fude_zoom_template* fzct_find(const fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    fude_zoom_template* _e = (fude_zoom_template*)_t->list.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->list); _i++) {
        if(_e[_i].kind == _kind && strcmp(_e[_i].key, _key) == 0) {
            return &_e[_i];
        }
    }
    return NULL;
}

u32 fude_zoom_templates_canvas(const fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    const fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    return _e != NULL ? _e->canvas : 0u;
}

const fude_zoom_template* fude_zoom_templates_of(const fude_zoom_templates* _t, u32 _canvas) {
    const fude_zoom_template* _e = (const fude_zoom_template*)_t->list.memory;
    for(u32 _i = 0; _canvas != 0u && _i < (u32)rde_arr_length(&_t->list); _i++) {
        if(_e[_i].canvas == _canvas) {
            return &_e[_i];
        }
    }
    return NULL;
}

void fude_zoom_templates_set(fude_zoom_templates* _t, u8 _kind, const c8* _key, u32 _canvas) {
    // (a key with a space would not be read back: none is made so — a piece's is a number, a part's a slug)
    if(_key == NULL || _key[0] == 0 || strlen(_key) >= FUDE_ZOOM_CUSTOM_KEY || strchr(_key, ' ') != NULL) {
        return;
    }
    fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    if(_e != NULL) {
        _e->canvas = _canvas;
        return;
    }
    fude_zoom_template _new = { _kind, { 0 }, _canvas };
    snprintf(_new.key, sizeof(_new.key), "%s", _key);
    rde_arr_add(&_t->list, (any)&_new);
}

u32 fude_zoom_templates_drop(fude_zoom_templates* _t, u8 _kind, const c8* _key) {
    fude_zoom_template* _e = fzct_find(_t, _kind, _key);
    if(_e == NULL) {
        return 0u;
    }
    const u32 _canvas = _e->canvas;
    rde_arr_remove(&_t->list, (u32)(_e - (fude_zoom_template*)_t->list.memory));
    return _canvas;
}

void fude_zoom_templates_rekey(fude_zoom_templates* _t, u8 _kind, const c8* _key, const c8* _to) {
    const u32 _canvas = fude_zoom_templates_drop(_t, _kind, _key);
    if(_canvas != 0u) {
        fude_zoom_templates_set(_t, _kind, _to, _canvas);
    }
}
