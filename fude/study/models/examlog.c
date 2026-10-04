// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/examlog.h"
#include "drawing/base/kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See examlog.h.
// ===========================================================================

#define FUDE_EXAMLOG_VERSION     1u
#define FUDE_EXAMLOG_KIND        FUDE_TAG('E', 'X', 'A', 'M')
#define FUDE_EXAMLOG_CHUNK       FUDE_TAG('E', 'X', 'A', 'M')
#define FUDE_EXAMLOG_ITEM_SIZE   24u

static rde_arr TYPE(fude_examlog_exam) fude_examlog_all;
static rde_arr TYPE(fude_examlog_item) fude_examlog_all_items;
static b8                              fude_examlog_ready = false;
static c8                              fude_examlog_path[RDE_MAX_PATH];
static u32                             fude_examlog_changes = 0;
static u32                             fude_examlog_chunks  = 0;   // exam chunks in the file, damaged ones too

RDE_INTERNAL void fude_examlog_ensure(void) {
    if(!fude_examlog_ready) {
        fude_examlog_all       = rde_arr_new(sizeof(fude_examlog_exam), rde_memory_allocator_get_default_std());
        fude_examlog_all_items = rde_arr_new(sizeof(fude_examlog_item), rde_memory_allocator_get_default_std());
        fude_examlog_ready     = true;
    }
}

// One exam's results into memory (the summary worked out from its items).
RDE_INTERNAL void fude_examlog_remember(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, u32 _chunk) {
    fude_examlog_exam _exam = { .time = _time, .source = _source, .first_item = (u32)rde_arr_length(&fude_examlog_all_items), .item_count = _count,
                                .chunk = _chunk };
    f32 _sum = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        rde_arr_add(&fude_examlog_all_items, &_items[_i]);
        _exam.correct += _items[_i].correct ? 1u : 0u;
        _sum          += _items[_i].score;
    }
    _exam.score = _count > 0 ? _sum / (f32)_count : 0.0f;
    rde_arr_add(&fude_examlog_all, &_exam);
}

void fude_examlog_open(const c8* _path) {
    fude_examlog_ensure();
    rde_arr_clear(&fude_examlog_all);
    rde_arr_clear(&fude_examlog_all_items);
    snprintf(fude_examlog_path, sizeof(fude_examlog_path), "%s", _path);
    fude_examlog_changes++;
    fude_examlog_chunks = 0;

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // no exams yet
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_from, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
        fude_file_free(_data);
        fude_file_set_aside(_from);
        return;
    }

    static fude_examlog_item _items[256];
    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_EXAMLOG_CHUNK) {
            continue;
        }
        const u32 _ordinal = fude_examlog_chunks++;
        const u32 _lo     = fude_get_u32(&_chunk);
        const u32 _hi     = fude_get_u32(&_chunk);
        const u8  _source = fude_get_u8(&_chunk);
        fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk);
        const u32 _count  = fude_get_u32(&_chunk);
        const u32 _record = fude_get_u32(&_chunk);
        if(!_chunk.ok || _record < 20u || _count > 256u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            continue;   // a damaged exam: the others still count
        }
        for(u32 _i = 0; _i < _count; _i++) {
            fude_reader _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            _items[_i].codepoint = fude_get_u32(&_e);
            _items[_i].correct   = fude_get_u8(&_e) != 0;
            fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
            _items[_i].score     = fude_get_f32(&_e);
            _items[_i].quality   = fude_get_f32(&_e);
            _items[_i].read_as   = fude_get_u32(&_e);
        }
        fude_examlog_remember((u64)_lo | ((u64)_hi << 32), _source, _items, _count, _ordinal);
    }
    fude_file_free(_data);
}

void fude_examlog_close(void) {
    if(fude_examlog_ready) {
        rde_arr_free(&fude_examlog_all);
        rde_arr_free(&fude_examlog_all_items);
        fude_examlog_ready = false;
    }
    fude_examlog_path[0] = 0;
    fude_examlog_changes++;
    fude_examlog_chunks = 0;
}

RDE_INTERNAL u16 fude_examlog_fraction(f32 _v, f32 _units) {
    const f32 _f = _units > 0.0f ? rde_math_clamp_f32(_v / _units, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

b8 fude_examlog_add(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, const fude_ink* const* _drawings, f32 _units) {
    fude_examlog_ensure();
    _count = _count > 256u ? 256u : _count;
    fude_examlog_remember(_time, _source, _items, _count, UINT32_MAX);
    fude_examlog_changes++;
    if(fude_examlog_path[0] == 0) {
        return true;   // in memory only
    }

    // What is there already, kept byte for byte: the log is only ever added to.
    u32 _old_size = 0;
    u8* _old      = rde_file_exists(fude_examlog_path) ? fude_file_read(fude_examlog_path, &_old_size) : NULL;
    if(_old != NULL) {
        fude_reader _r = fude_reader_make(_old, _old_size);
        if(!fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
            fude_file_free(_old);   // damaged: set aside, not lost nor overwritten
            _old = NULL;
            fude_file_set_aside(fude_examlog_path);
        }
    }

    fude_bytes _b = fude_bytes_new((_old != NULL ? _old_size : 0u) + 1024u + _count * 64u);
    if(_old != NULL) {
        fude_put_data(&_b, _old, _old_size);
        fude_file_free(_old);
    } else {
        // A new file: the exams read from an old one are no longer in it.
        fude_put_header(&_b, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND);
        fude_examlog_exam* _exams = (fude_examlog_exam*)fude_examlog_all.memory;
        for(u32 _e = 0; _e < (u32)rde_arr_length(&fude_examlog_all); _e++) {
            _exams[_e].chunk = UINT32_MAX;
        }
        fude_examlog_chunks = 0;
    }
    ((fude_examlog_exam*)fude_examlog_all.memory)[rde_arr_length(&fude_examlog_all) - 1u].chunk = fude_examlog_chunks++;

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_EXAMLOG_CHUNK);
    fude_put_u32(&_b, (u32)(_time & 0xFFFFFFFFu));
    fude_put_u32(&_b, (u32)(_time >> 32));
    fude_put_u8(&_b, _source);
    fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u);
    fude_put_u32(&_b, _count);
    fude_put_u32(&_b, FUDE_EXAMLOG_ITEM_SIZE);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_put_u32(&_b, _items[_i].codepoint);
        fude_put_u8(&_b, _items[_i].correct ? 1u : 0u);
        fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u);
        fude_put_f32(&_b, _items[_i].score);
        fude_put_f32(&_b, _items[_i].quality);
        fude_put_u32(&_b, _items[_i].read_as);
        fude_put_u32(&_b, 0u);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_ink* _ink = _drawings != NULL ? _drawings[_i] : NULL;
        u32 _strokes = 0;
        for(u32 _s = 0; _ink != NULL && _s < fude_ink_stroke_count(_ink); _s++) {
            _strokes += fude_ink_stroke_at(_ink, _s)->alive ? 1u : 0u;
        }
        fude_put_u16(&_b, (u16)(_strokes < 65535u ? _strokes : 65535u));
        for(u32 _s = 0; _ink != NULL && _s < fude_ink_stroke_count(_ink); _s++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
            if(!_stroke->alive) {
                continue;
            }
            const u32 _n = _stroke->point_count < 65535u ? _stroke->point_count : 65535u;
            const fude_ink_point* _p = fude_ink_stroke_points(_ink, _stroke);
            fude_put_u16(&_b, (u16)_n);
            for(u32 _k = 0; _k < _n; _k++) {
                fude_put_u16(&_b, fude_examlog_fraction(_p[_k].position.x, _units));
                fude_put_u16(&_b, fude_examlog_fraction(_p[_k].position.y, _units));
            }
        }
    }
    fude_chunk_end(&_b, _chunk);
    return fude_bytes_write_and_free(&_b, fude_examlog_path, NULL);
}

u32 fude_examlog_count(void) {
    return fude_examlog_ready ? (u32)rde_arr_length(&fude_examlog_all) : 0u;
}

const fude_examlog_exam* fude_examlog_exams(void) {
    return fude_examlog_ready ? (const fude_examlog_exam*)fude_examlog_all.memory : NULL;
}

const fude_examlog_item* fude_examlog_items(void) {
    return fude_examlog_ready ? (const fude_examlog_item*)fude_examlog_all_items.memory : NULL;
}

u32 fude_examlog_streak(u32 _codepoint) {
    const fude_examlog_exam* _exams = fude_examlog_exams();
    const fude_examlog_item* _items = fude_examlog_items();
    u32 _streak = 0;
    for(u32 _e = fude_examlog_count(); _e-- > 0;) {
        for(u32 _i = 0; _i < _exams[_e].item_count; _i++) {
            const fude_examlog_item* _it = &_items[_exams[_e].first_item + _i];
            if(_it->codepoint != _codepoint) {
                continue;
            }
            if(!_it->correct) {
                return _streak;
            }
            _streak++;
            break;   // once per exam
        }
    }
    return _streak;
}

u32 fude_examlog_revision(void) {
    return fude_examlog_changes;
}

u32 fude_examlog_read_writing(u32 _exam, u32 _codepoint, rde_arr* _writings, rde_arr* _strokes, rde_arr* _points) {
    const u32 _count = fude_examlog_count();
    if(_count == 0 || fude_examlog_path[0] == 0 || !rde_file_exists(fude_examlog_path)) {
        return 0;
    }
    // Which exam each chunk of the file is.
    const fude_examlog_exam* _exams    = fude_examlog_exams();
    const fude_examlog_item* _items    = fude_examlog_items();
    u32*                     _by_chunk = (u32*)rde_malloc(sizeof(u32) * (fude_examlog_chunks + 1u));
    for(u32 _c = 0; _c <= fude_examlog_chunks; _c++) {
        _by_chunk[_c] = UINT32_MAX;
    }
    for(u32 _e = 0; _e < _count; _e++) {
        if(_exams[_e].chunk < fude_examlog_chunks && (_exam == UINT32_MAX || _e == _exam)) {
            _by_chunk[_exams[_e].chunk] = _e;
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(fude_examlog_path, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    u32 _found = 0;
    if(_data != NULL && fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
        u32         _tag;
        fude_reader _chunk;
        u32         _ordinal = 0;
        while(fude_next_chunk(&_r, &_tag, &_chunk)) {
            if(_tag != FUDE_EXAMLOG_CHUNK) {
                continue;
            }
            const u32 _e = _ordinal < fude_examlog_chunks ? _by_chunk[_ordinal] : UINT32_MAX;
            _ordinal++;
            if(_e == UINT32_MAX) {
                continue;
            }
            fude_get_u32(&_chunk); fude_get_u32(&_chunk);                       // time
            fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk);
            const u32 _n      = fude_get_u32(&_chunk);
            const u32 _record = fude_get_u32(&_chunk);
            if(!_chunk.ok || _n != _exams[_e].item_count || (u64)_n * _record > (u64)(_chunk.size - _chunk.pos)) {
                continue;   // not the exam remembered: leave it
            }
            _chunk.pos += _n * _record;
            for(u32 _i = 0; _i < _n && _chunk.ok; _i++) {
                const b8  _wanted  = _codepoint == 0 || _items[_exams[_e].first_item + _i].codepoint == _codepoint;
                const u32 _nstroke = fude_get_u16(&_chunk);
                fude_examlog_writing _w = { .exam = _e, .item = _i, .first_stroke = (u32)rde_arr_length(_strokes), .stroke_count = 0 };
                for(u32 _s = 0; _s < _nstroke && _chunk.ok; _s++) {
                    const u32 _np = fude_get_u16(&_chunk);
                    if(!fude_reader_has(&_chunk, _np * 4u)) {
                        _chunk.ok = false;
                        break;
                    }
                    if(!_wanted || _np == 0) {
                        _chunk.pos += _np * 4u;
                        continue;
                    }
                    const fude_history_stroke _stroke = { .first_point = (u32)rde_arr_length(_points), .point_count = _np };
                    fude_history_point*       _p      = (fude_history_point*)rde_arr_add_n(_points, _np);
                    for(u32 _k = 0; _k < _np; _k++) {
                        _p[_k].x    = fude_get_u16(&_chunk);
                        _p[_k].y    = fude_get_u16(&_chunk);
                        _p[_k].time = (f32)_k * FUDE_EXAMLOG_POINT_SECONDS;
                    }
                    rde_arr_add(_strokes, &_stroke);
                    _w.stroke_count++;
                }
                if(_wanted && _chunk.ok) {
                    rde_arr_add(_writings, &_w);
                    _found++;
                }
            }
        }
    }
    fude_file_free(_data);
    rde_free(_by_chunk);
    return _found;
}
